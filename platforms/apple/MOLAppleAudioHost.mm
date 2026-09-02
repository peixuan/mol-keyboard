// SPDX-License-Identifier: Apache-2.0
#import "MOLAppleAudioHost.h"

#import <AudioToolbox/AudioToolbox.h>
#import <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_VISION
#import <AVFAudio/AVFAudio.h>
#define MOL_APPLE_HAS_AUDIO_SESSION 1
#else
#define MOL_APPLE_HAS_AUDIO_SESSION 0
#endif

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "mol_platform/audio_runtime.h"

NSErrorDomain const MOLAppleAudioErrorDomain = @"cn.zhangpeixuan.molkeyboard.apple-audio";

namespace {

constexpr UInt32 kChannelCount = 2U;
constexpr Float64 kFallbackSampleRate = 48000.0;
constexpr NSTimeInterval kPreferredBufferDuration = 128.0 / kFallbackSampleRate;

struct AppleAudioState {
  mol_platform_audio_runtime_t runtime{};
  AudioUnit unit = nullptr;
  std::atomic<std::uint64_t> callback_count{0};
  std::atomic<std::uint64_t> rendered_frames{0};
  std::atomic<std::uint32_t> render_failures{0};
  std::atomic<std::uint32_t> non_finite_samples{0};
  std::atomic<std::uint32_t> route_changes{0};
  std::atomic<std::uint32_t> interruptions{0};
  std::atomic<std::uint32_t> sample_rate{0};
  std::atomic<std::uint32_t> maximum_frames_per_slice{0};
  std::atomic<std::int32_t> last_status{noErr};
  std::atomic<bool> runtime_ready{false};
  std::atomic<bool> active{false};
  std::atomic<bool> media_services_reset{false};
};

void silence(AudioBufferList* buffers) noexcept {
  if (buffers == nullptr) {
    return;
  }
  for (UInt32 index = 0U; index < buffers->mNumberBuffers; ++index) {
    AudioBuffer& buffer = buffers->mBuffers[index];
    if (buffer.mData != nullptr && buffer.mDataByteSize > 0U) {
      std::memset(buffer.mData, 0, buffer.mDataByteSize);
    }
  }
}

OSStatus render_callback(void* context, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32,
                         UInt32 frame_count, AudioBufferList* buffers) noexcept {
  auto* state = static_cast<AppleAudioState*>(context);
  if (state == nullptr || buffers == nullptr || frame_count == 0U) {
    silence(buffers);
    return noErr;
  }
  if (!state->active.load(std::memory_order_acquire) ||
      !state->runtime_ready.load(std::memory_order_acquire)) {
    silence(buffers);
    return noErr;
  }

  const std::size_t required_bytes =
      static_cast<std::size_t>(frame_count) * kChannelCount * sizeof(float);
  if (buffers->mNumberBuffers != 1U || buffers->mBuffers[0].mData == nullptr ||
      buffers->mBuffers[0].mNumberChannels != kChannelCount ||
      buffers->mBuffers[0].mDataByteSize < required_bytes) {
    silence(buffers);
    state->render_failures.fetch_add(1U, std::memory_order_relaxed);
    return noErr;
  }

  auto* output = static_cast<float*>(buffers->mBuffers[0].mData);
  const mol_result_t result = mol_platform_audio_render_f32(&state->runtime, output, frame_count);
  if (result != MOL_OK) {
    silence(buffers);
    state->render_failures.fetch_add(1U, std::memory_order_relaxed);
  } else {
    const std::size_t sample_count = static_cast<std::size_t>(frame_count) * kChannelCount;
    for (std::size_t index = 0; index < sample_count; ++index) {
      if (!std::isfinite(output[index])) {
        output[index] = 0.0F;
        state->non_finite_samples.fetch_add(1U, std::memory_order_relaxed);
      }
    }
  }
  state->callback_count.fetch_add(1U, std::memory_order_relaxed);
  state->rendered_frames.fetch_add(frame_count, std::memory_order_relaxed);
  return noErr;
}

NSError* make_error(OSStatus status, NSString* operation) {
  return [NSError errorWithDomain:MOLAppleAudioErrorDomain
                             code:status
                         userInfo:@{
                           NSLocalizedDescriptionKey :
                               [NSString stringWithFormat:@"%@ failed with OSStatus %d", operation,
                                                          static_cast<int>(status)]
                         }];
}

void dispose_unit(AppleAudioState* state) noexcept {
  state->active.store(false, std::memory_order_release);
  if (state->unit != nullptr) {
    (void)AudioOutputUnitStop(state->unit);
    (void)AudioUnitUninitialize(state->unit);
    (void)AudioComponentInstanceDispose(state->unit);
    state->unit = nullptr;
  }
  state->runtime_ready.store(false, std::memory_order_release);
  mol_platform_audio_shutdown(&state->runtime);
  state->sample_rate.store(0U, std::memory_order_release);
  state->maximum_frames_per_slice.store(0U, std::memory_order_release);
}

mol_result_t submit_note(AppleAudioState* state, std::uint32_t command_type, std::uint8_t note,
                         float velocity, std::uint64_t gesture_id) noexcept {
  mol_command_t command{};
  if (state == nullptr || !state->runtime_ready.load(std::memory_order_acquire) || note > 127U ||
      !std::isfinite(velocity) || velocity < 0.0F || velocity > 1.0F) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command.struct_size = sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = command_type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = gesture_id;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  return mol_platform_audio_submit(&state->runtime, &command);
}

}  // namespace

@implementation MOLAppleAudioHost {
  AppleAudioState* _state;
  BOOL _resumeAfterInterruption;
}

- (instancetype)init {
  self = [super init];
  if (self != nil) {
    _state = new (std::nothrow) AppleAudioState();
    if (_state == nullptr) {
      return nil;
    }
#if MOL_APPLE_HAS_AUDIO_SESSION
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    AVAudioSession* session = [AVAudioSession sharedInstance];
    [center addObserver:self
               selector:@selector(handleInterruption:)
                   name:AVAudioSessionInterruptionNotification
                 object:session];
    [center addObserver:self
               selector:@selector(handleRouteChange:)
                   name:AVAudioSessionRouteChangeNotification
                 object:session];
    [center addObserver:self
               selector:@selector(handleMediaServicesReset:)
                   name:AVAudioSessionMediaServicesWereResetNotification
                 object:session];
#endif
  }
  return self;
}

- (void)dealloc {
#if MOL_APPLE_HAS_AUDIO_SESSION
  [[NSNotificationCenter defaultCenter] removeObserver:self];
#endif
  [self stop];
  delete _state;
  _state = nullptr;
}

- (BOOL)startWithError:(NSError**)error {
  if (_state == nullptr) {
    if (error != nullptr) {
      *error = make_error(kAudio_ParamError, @"state allocation");
    }
    return NO;
  }
  dispose_unit(_state);

  Float64 sample_rate = kFallbackSampleRate;
#if MOL_APPLE_HAS_AUDIO_SESSION
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSError* session_error = nil;
  if (![session setCategory:AVAudioSessionCategoryPlayback
                       mode:AVAudioSessionModeDefault
                    options:AVAudioSessionCategoryOptionAllowAirPlay
                      error:&session_error] ||
      ![session setPreferredIOBufferDuration:kPreferredBufferDuration error:&session_error] ||
      ![session setActive:YES error:&session_error]) {
    if (error != nullptr) {
      *error = session_error;
    }
    return NO;
  }
  sample_rate = session.sampleRate;
#endif

  AudioComponentDescription description{};
  description.componentType = kAudioUnitType_Output;
#if MOL_APPLE_HAS_AUDIO_SESSION
  description.componentSubType = kAudioUnitSubType_RemoteIO;
#else
  description.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
  description.componentManufacturer = kAudioUnitManufacturer_Apple;
  AudioComponent component = AudioComponentFindNext(nullptr, &description);
  OSStatus status = component == nullptr
                        ? kAudio_ParamError
                        : AudioComponentInstanceNew(component, &_state->unit);
  if (status != noErr) {
    _state->last_status.store(status, std::memory_order_release);
    if (error != nullptr) {
      *error = make_error(status, @"AudioComponentInstanceNew");
    }
    dispose_unit(_state);
    return NO;
  }

#if !MOL_APPLE_HAS_AUDIO_SESSION
  AudioStreamBasicDescription hardware_format{};
  UInt32 hardware_format_size = sizeof(hardware_format);
  status = AudioUnitGetProperty(_state->unit, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output, 0U, &hardware_format,
                                &hardware_format_size);
  if (status == noErr && hardware_format.mSampleRate > 0.0) {
    sample_rate = hardware_format.mSampleRate;
  }
#endif
  if (sample_rate <= 0.0) {
    sample_rate = kFallbackSampleRate;
  }

  AudioStreamBasicDescription format{};
  format.mSampleRate = sample_rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
  format.mBytesPerPacket = kChannelCount * sizeof(float);
  format.mFramesPerPacket = 1U;
  format.mBytesPerFrame = kChannelCount * sizeof(float);
  format.mChannelsPerFrame = kChannelCount;
  format.mBitsPerChannel = 8U * sizeof(float);
  status = AudioUnitSetProperty(_state->unit, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input, 0U, &format, sizeof(format));
  if (status == noErr) {
    AURenderCallbackStruct callback{render_callback, _state};
    status = AudioUnitSetProperty(_state->unit, kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input, 0U, &callback, sizeof(callback));
  }

  UInt32 maximum_frames = 0U;
  UInt32 maximum_frames_size = sizeof(maximum_frames);
  if (status == noErr) {
    status = AudioUnitGetProperty(_state->unit, kAudioUnitProperty_MaximumFramesPerSlice,
                                  kAudioUnitScope_Global, 0U, &maximum_frames,
                                  &maximum_frames_size);
  }
  if (status == noErr) {
    const auto integer_sample_rate = static_cast<std::uint32_t>(std::llround(sample_rate));
    const mol_result_t engine_result =
        mol_platform_audio_init(&_state->runtime, integer_sample_rate, kChannelCount);
    if (engine_result == MOL_OK) {
      _state->sample_rate.store(integer_sample_rate, std::memory_order_release);
      _state->maximum_frames_per_slice.store(maximum_frames, std::memory_order_release);
      _state->runtime_ready.store(true, std::memory_order_release);
    } else {
      status = kAudio_ParamError;
    }
  }
  if (status == noErr) {
    status = AudioUnitInitialize(_state->unit);
  }
  if (status == noErr) {
    _state->active.store(true, std::memory_order_release);
    status = AudioOutputUnitStart(_state->unit);
  }
  if (status != noErr) {
    _state->last_status.store(status, std::memory_order_release);
    if (error != nullptr) {
      *error = make_error(status, @"AudioUnit startup");
    }
    dispose_unit(_state);
    return NO;
  }

  _state->media_services_reset.store(false, std::memory_order_release);
  _state->last_status.store(noErr, std::memory_order_release);
  return YES;
}

- (void)stop {
  if (_state != nullptr) {
    dispose_unit(_state);
  }
#if MOL_APPLE_HAS_AUDIO_SESSION
  NSError* error = nil;
  [[AVAudioSession sharedInstance]
      setActive:NO
      withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
            error:&error];
#endif
}

- (int32_t)noteOn:(uint8_t)note velocity:(float)velocity gestureId:(uint64_t)gestureId {
  return submit_note(_state, MOL_COMMAND_NOTE_ON, note, velocity, gestureId);
}

- (int32_t)noteOff:(uint8_t)note gestureId:(uint64_t)gestureId {
  return submit_note(_state, MOL_COMMAND_NOTE_OFF, note, 0.0F, gestureId);
}

- (MOLAppleAudioStatus)status {
  if (_state == nullptr) {
    return MOLAppleAudioStatus{};
  }
  return MOLAppleAudioStatus{
      _state->sample_rate.load(std::memory_order_acquire),
      _state->maximum_frames_per_slice.load(std::memory_order_acquire),
      _state->callback_count.load(std::memory_order_relaxed),
      _state->rendered_frames.load(std::memory_order_relaxed),
      _state->render_failures.load(std::memory_order_relaxed),
      _state->non_finite_samples.load(std::memory_order_relaxed),
      _state->route_changes.load(std::memory_order_relaxed),
      _state->interruptions.load(std::memory_order_relaxed),
      _state->last_status.load(std::memory_order_acquire),
      _state->active.load(std::memory_order_acquire),
      _state->media_services_reset.load(std::memory_order_acquire),
  };
}

#if MOL_APPLE_HAS_AUDIO_SESSION
- (void)handleInterruption:(NSNotification*)notification {
  NSNumber* type_value = notification.userInfo[AVAudioSessionInterruptionTypeKey];
  NSNumber* options_value = notification.userInfo[AVAudioSessionInterruptionOptionKey];
  _state->interruptions.fetch_add(1U, std::memory_order_relaxed);
  if (type_value.unsignedIntegerValue == AVAudioSessionInterruptionTypeBegan) {
    _resumeAfterInterruption = _state->active.load(std::memory_order_acquire);
    [self performSelectorOnMainThread:@selector(stop) withObject:nil waitUntilDone:NO];
  } else if (_resumeAfterInterruption &&
             (options_value.unsignedIntegerValue & AVAudioSessionInterruptionOptionShouldResume) !=
                 0U) {
    _resumeAfterInterruption = NO;
    [self performSelectorOnMainThread:@selector(restartAfterSystemChange)
                           withObject:nil
                        waitUntilDone:NO];
  }
}

- (void)handleRouteChange:(NSNotification*)notification {
  (void)notification;
  _state->route_changes.fetch_add(1U, std::memory_order_relaxed);
  if (_state->active.load(std::memory_order_acquire)) {
    [self performSelectorOnMainThread:@selector(restartAfterSystemChange)
                           withObject:nil
                        waitUntilDone:NO];
  }
}

- (void)handleMediaServicesReset:(NSNotification*)notification {
  (void)notification;
  _state->media_services_reset.store(true, std::memory_order_release);
  [self performSelectorOnMainThread:@selector(stop) withObject:nil waitUntilDone:NO];
}

- (void)restartAfterSystemChange {
  if (_state->active.load(std::memory_order_acquire)) {
    [self stop];
    (void)[self startWithError:nil];
  }
}
#endif

@end
