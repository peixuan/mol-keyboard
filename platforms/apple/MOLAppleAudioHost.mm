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
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <vector>

#include "mol_platform/audio_runtime.h"

NSErrorDomain const MOLAppleAudioErrorDomain = @"cn.zhangpeixuan.molkeyboard.apple-audio";
NSNotificationName const MOLAppleAudioHostDidRestartNotification =
    @"MOLAppleAudioHostDidRestartNotification";
NSNotificationName const MOLAppleAudioHostMediaServicesResetNotification =
    @"MOLAppleAudioHostMediaServicesResetNotification";

namespace {

constexpr UInt32 kChannelCount = 2U;
constexpr Float64 kFallbackSampleRate = 48000.0;
constexpr NSTimeInterval kPreferredBufferDuration = 128.0 / kFallbackSampleRate;
constexpr std::size_t kMaximumSequenceBytes = 2U * 1024U * 1024U;

struct AppleAudioState {
  mol_platform_audio_runtime_t runtime{};
  AudioUnit unit = nullptr;
  std::atomic<std::uint64_t> callback_count{0};
  std::atomic<std::uint64_t> rendered_frames{0};
  std::atomic<std::uint32_t> render_failures{0};
  std::atomic<std::uint32_t> non_finite_samples{0};
  std::atomic<std::uint32_t> route_changes{0};
  std::atomic<std::uint32_t> interruptions{0};
  std::atomic<std::uint32_t> callbacks_in_flight{0};
  std::atomic<std::uint32_t> sample_rate{0};
  std::atomic<std::uint32_t> maximum_frames_per_slice{0};
  std::atomic<std::int32_t> last_status{noErr};
  std::atomic<bool> runtime_ready{false};
  std::atomic<bool> active{false};
  std::atomic<bool> media_services_reset{false};
};

struct SequenceWriter {
  std::vector<std::uint8_t>* bytes;
};

struct SequenceReader {
  const std::uint8_t* bytes;
  std::size_t size;
  std::size_t offset;
  mol_sequence_event_t* events;
  std::uint32_t event_capacity;
  std::uint32_t event_count;
};

mol_result_t write_sequence(void* user_data, const std::uint8_t* data, std::size_t size) {
  auto* writer = static_cast<SequenceWriter*>(user_data);
  if (writer == nullptr || writer->bytes == nullptr || data == nullptr ||
      writer->bytes->size() > kMaximumSequenceBytes ||
      size > kMaximumSequenceBytes - writer->bytes->size()) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  try {
    writer->bytes->insert(writer->bytes->end(), data, data + size);
  } catch (...) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
  return MOL_OK;
}

std::size_t read_sequence(void* user_data, std::uint8_t* data, std::size_t capacity) {
  auto* reader = static_cast<SequenceReader*>(user_data);
  if (reader == nullptr || data == nullptr || reader->offset > reader->size) return 0U;
  const std::size_t remaining = reader->size - reader->offset;
  const std::size_t copy_size = capacity < remaining ? capacity : remaining;
  std::memcpy(data, reader->bytes + reader->offset, copy_size);
  reader->offset += copy_size;
  return copy_size;
}

mol_result_t collect_sequence_event(void* user_data, const mol_sequence_event_t* event) {
  auto* reader = static_cast<SequenceReader*>(user_data);
  if (reader == nullptr || event == nullptr || reader->events == nullptr ||
      reader->event_count >= reader->event_capacity) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  reader->events[reader->event_count++] = *event;
  return MOL_OK;
}

mol_command_t make_command(std::uint32_t command_type, std::uint64_t gesture_id) {
  mol_command_t command{};
  command.struct_size = sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = command_type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = gesture_id;
  return command;
}

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
  state->callbacks_in_flight.fetch_add(1U, std::memory_order_acq_rel);
  if (!state->active.load(std::memory_order_acquire) ||
      !state->runtime_ready.load(std::memory_order_acquire)) {
    silence(buffers);
    state->callbacks_in_flight.fetch_sub(1U, std::memory_order_release);
    return noErr;
  }

  const std::size_t required_bytes =
      static_cast<std::size_t>(frame_count) * kChannelCount * sizeof(float);
  if (buffers->mNumberBuffers != 1U || buffers->mBuffers[0].mData == nullptr ||
      buffers->mBuffers[0].mNumberChannels != kChannelCount ||
      buffers->mBuffers[0].mDataByteSize < required_bytes) {
    silence(buffers);
    state->render_failures.fetch_add(1U, std::memory_order_relaxed);
    state->callbacks_in_flight.fetch_sub(1U, std::memory_order_release);
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
  state->callbacks_in_flight.fetch_sub(1U, std::memory_order_release);
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

mol_result_t submit_control(AppleAudioState* state, std::uint32_t command_type,
                            std::uint64_t gesture_id, std::int32_t integer_0,
                            std::int32_t integer_1, std::int32_t integer_2, std::int32_t integer_3,
                            float scalar_0, float scalar_1) noexcept {
  if (state == nullptr || !state->runtime_ready.load(std::memory_order_acquire) ||
      !std::isfinite(scalar_0) || !std::isfinite(scalar_1)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  mol_command_t command = make_command(command_type, gesture_id);
  switch (command_type) {
    case MOL_COMMAND_NOTE_ON:
      if (integer_0 < 0 || integer_0 > 127 || scalar_0 < 0.0F || scalar_0 > 1.0F) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.note.note = static_cast<std::uint8_t>(integer_0);
      command.payload.note.velocity = scalar_0;
      break;
    case MOL_COMMAND_NOTE_OFF:
      if (integer_0 < 0 || integer_0 > 127) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.note.note = static_cast<std::uint8_t>(integer_0);
      break;
    case MOL_COMMAND_SUSTAIN:
    case MOL_COMMAND_SET_MASTER_GAIN:
    case MOL_COMMAND_SET_TEMPO:
      command.payload.scalar.value = scalar_0;
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
    case MOL_COMMAND_SET_TRANSPOSE:
    case MOL_COMMAND_SET_CHORD_MODE:
      command.payload.integer.value = integer_0;
      break;
    case MOL_COMMAND_SET_PRESET:
      if (integer_0 < 0) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.preset.preset = static_cast<std::uint32_t>(integer_0);
      command.payload.preset.hard_switch = integer_1 != 0 ? 1U : 0U;
      break;
    case MOL_COMMAND_SET_PARAMETER:
      if (integer_0 < 0) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.parameter.parameter = static_cast<std::uint32_t>(integer_0);
      command.payload.parameter.value = scalar_0;
      break;
    case MOL_COMMAND_SET_SCALE:
      if (integer_0 < 0 || integer_1 < 0 || integer_1 > 11 || integer_2 < 0 ||
          integer_2 > std::numeric_limits<std::uint8_t>::max()) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.scale.type = static_cast<std::uint32_t>(integer_0);
      command.payload.scale.tonic = static_cast<std::uint8_t>(integer_1);
      command.payload.scale.mapping = static_cast<std::uint8_t>(integer_2);
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      if (integer_0 < 0 || integer_1 < 0 || integer_2 < 0 ||
          integer_2 > std::numeric_limits<std::uint8_t>::max() || scalar_0 < 0.0F) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.arpeggiator.mode = static_cast<std::uint32_t>(integer_0);
      command.payload.arpeggiator.rate = static_cast<std::uint32_t>(integer_1);
      command.payload.arpeggiator.octaves = static_cast<std::uint8_t>(integer_2);
      command.payload.arpeggiator.random_seed = static_cast<std::uint32_t>(integer_3);
      command.payload.arpeggiator.gate = scalar_0;
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      if (integer_0 <= 0 || integer_0 > std::numeric_limits<std::uint8_t>::max() ||
          integer_1 <= 0 || integer_1 > std::numeric_limits<std::uint8_t>::max()) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.time_signature.numerator = static_cast<std::uint8_t>(integer_0);
      command.payload.time_signature.denominator = static_cast<std::uint8_t>(integer_1);
      break;
    case MOL_COMMAND_SET_METRONOME:
      command.payload.metronome.enabled = integer_0 != 0 ? 1U : 0U;
      command.payload.metronome.level = scalar_0;
      break;
    case MOL_COMMAND_SET_PORTAMENTO:
      if (integer_0 < 0 || scalar_0 < 0.0F) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.portamento.mode = static_cast<std::uint32_t>(integer_0);
      command.payload.portamento.time_ms = scalar_0;
      break;
    case MOL_COMMAND_ALL_NOTES_OFF:
    case MOL_COMMAND_ALL_SOUND_OFF:
    case MOL_COMMAND_TRANSPORT_START:
    case MOL_COMMAND_TRANSPORT_STOP:
    case MOL_COMMAND_RECORD_START:
    case MOL_COMMAND_RECORD_STOP:
    case MOL_COMMAND_PLAYBACK_START:
    case MOL_COMMAND_PLAYBACK_STOP:
    case MOL_COMMAND_RESET_ENGINE:
      break;
    default:
      return MOL_ERROR_INVALID_ARGUMENT;
  }
  return mol_platform_audio_submit(&state->runtime, &command);
}

}  // namespace

#if MOL_APPLE_HAS_AUDIO_SESSION
@interface MOLAppleAudioHost ()

- (void)handleInterruption:(NSNotification*)notification;
- (void)handleRouteChange:(NSNotification*)notification;
- (void)handleMediaServicesReset:(NSNotification*)notification;
- (void)restartAfterSystemChange;
- (void)handleMediaServicesResetOnMain;

@end
#endif

@implementation MOLAppleAudioHost {
  AppleAudioState* _state;
  BOOL _resumeAfterInterruption;
  BOOL _restartRequested;
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
      ![session setPreferredSampleRate:kFallbackSampleRate error:&session_error] ||
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
  OSStatus status = component == nullptr ? kAudio_ParamError
                                         : AudioComponentInstanceNew(component, &_state->unit);
  if (status != noErr) {
    _state->last_status.store(status, std::memory_order_release);
    if (error != nullptr) {
      *error = make_error(status, @"AudioComponentInstanceNew");
    }
    dispose_unit(_state);
#if MOL_APPLE_HAS_AUDIO_SESSION
    [[AVAudioSession sharedInstance]
          setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
              error:nil];
#endif
    return NO;
  }

#if !MOL_APPLE_HAS_AUDIO_SESSION
  AudioStreamBasicDescription hardware_format{};
  UInt32 hardware_format_size = sizeof(hardware_format);
  status =
      AudioUnitGetProperty(_state->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                           0U, &hardware_format, &hardware_format_size);
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
    status =
        AudioUnitGetProperty(_state->unit, kAudioUnitProperty_MaximumFramesPerSlice,
                             kAudioUnitScope_Global, 0U, &maximum_frames, &maximum_frames_size);
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
#if MOL_APPLE_HAS_AUDIO_SESSION
    [[AVAudioSession sharedInstance]
          setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
              error:nil];
#endif
    return NO;
  }

  _state->media_services_reset.store(false, std::memory_order_release);
  _state->last_status.store(noErr, std::memory_order_release);
  return YES;
}

- (void)stop {
  _resumeAfterInterruption = NO;
  _restartRequested = NO;
  if (_state != nullptr) {
    dispose_unit(_state);
  }
#if MOL_APPLE_HAS_AUDIO_SESSION
  NSError* error = nil;
  [[AVAudioSession sharedInstance] setActive:NO
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

- (int32_t)submitCommandType:(uint32_t)commandType
                   gestureId:(uint64_t)gestureId
                    integer0:(int32_t)integer0
                    integer1:(int32_t)integer1
                    integer2:(int32_t)integer2
                    integer3:(int32_t)integer3
                     scalar0:(float)scalar0
                     scalar1:(float)scalar1 {
  return submit_control(_state, commandType, gestureId, integer0, integer1, integer2, integer3,
                        scalar0, scalar1);
}

- (NSArray<NSNumber*>*)pollEvents {
  constexpr std::uint32_t kMaximumEvents = 64U;
  constexpr std::uint32_t kFieldsPerEvent = 5U;
  if (_state == nullptr || !_state->runtime_ready.load(std::memory_order_acquire) ||
      _state->runtime.engine == nullptr) {
    return @[];
  }
  mol_event_t events[kMaximumEvents]{};
  const std::uint32_t count =
      mol_engine_poll_events(_state->runtime.engine, events, kMaximumEvents);
  NSMutableArray<NSNumber*>* fields = [NSMutableArray arrayWithCapacity:count * kFieldsPerEvent];
  for (std::uint32_t index = 0U; index < count; ++index) {
    const mol_event_t& event = events[index];
    [fields addObject:@(event.event_type)];
    [fields addObject:@(event.gesture_id)];
    [fields addObject:@(event.frame)];
    [fields addObject:@(event.payload[MOL_EVENT_PAYLOAD_NOTE])];
    [fields addObject:@(event.payload[0])];
  }
  return fields;
}

- (NSData*)exportRecording {
  if (_state == nullptr || !_state->runtime_ready.load(std::memory_order_acquire) ||
      _state->runtime.engine == nullptr) {
    return nil;
  }
  try {
    std::vector<mol_sequence_event_t> events(MOL_PROFILE_SEQUENCE_EVENTS);
    mol_sequence_config_t config{};
    config.struct_size = sizeof(config);
    config.api_version = MOL_API_VERSION;
    std::uint32_t event_count = 0U;
    mol_result_t result =
        mol_engine_copy_recording(_state->runtime.engine, &config, events.data(),
                                  static_cast<std::uint32_t>(events.size()), &event_count);
    if (result != MOL_OK || event_count == 0U) return nil;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(4096U);
    SequenceWriter destination{&bytes};
    mol_sequence_writer_t writer{};
    writer.struct_size = sizeof(writer);
    writer.api_version = MOL_API_VERSION;
    result = mol_sequence_writer_init(&writer, &config, write_sequence, &destination);
    for (std::uint32_t index = 0U; result == MOL_OK && index < event_count; ++index) {
      result = mol_sequence_writer_append(&writer, &events[index]);
    }
    if (result == MOL_OK) result = mol_sequence_writer_finalize(&writer);
    if (result != MOL_OK || bytes.empty()) return nil;
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
  } catch (...) {
    return nil;
  }
}

- (int32_t)loadRecording:(NSData*)data {
  if (_state == nullptr || !_state->runtime_ready.load(std::memory_order_acquire) ||
      _state->runtime.engine == nullptr || data.length == 0U ||
      data.length > kMaximumSequenceBytes) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::vector<mol_sequence_event_t> events(MOL_PROFILE_SEQUENCE_EVENTS);
    SequenceReader source{
        static_cast<const std::uint8_t*>(data.bytes), data.length, 0U, events.data(),
        static_cast<std::uint32_t>(events.size()),    0U};
    mol_sequence_callbacks_t callbacks{};
    callbacks.struct_size = sizeof(callbacks);
    callbacks.api_version = MOL_API_VERSION;
    callbacks.on_event = collect_sequence_event;
    callbacks.user_data = &source;
    mol_sequence_config_t config{};
    config.struct_size = sizeof(config);
    config.api_version = MOL_API_VERSION;
    const mol_result_t read_result =
        mol_sequence_read_stream(read_sequence, &source, &config, &callbacks);
    if (read_result != MOL_OK || source.offset != source.size) return read_result;

    const bool was_active = _state->active.exchange(false, std::memory_order_acq_rel);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (_state->callbacks_in_flight.load(std::memory_order_acquire) != 0U &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    if (_state->callbacks_in_flight.load(std::memory_order_acquire) != 0U) {
      _state->active.store(was_active, std::memory_order_release);
      return MOL_ERROR_INVALID_STATE;
    }
    const mol_result_t load_result = mol_engine_load_sequence(_state->runtime.engine, &config,
                                                              events.data(), source.event_count);
    _state->active.store(was_active, std::memory_order_release);
    return load_result;
  } catch (...) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
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
    const BOOL should_resume = _state->active.load(std::memory_order_acquire);
    [self performSelectorOnMainThread:@selector(stop) withObject:nil waitUntilDone:YES];
    _resumeAfterInterruption = should_resume;
  } else {
    const BOOL should_resume =
        _resumeAfterInterruption &&
        (options_value.unsignedIntegerValue & AVAudioSessionInterruptionOptionShouldResume) != 0U;
    _resumeAfterInterruption = NO;
    if (should_resume) {
      _restartRequested = YES;
      [self performSelectorOnMainThread:@selector(restartAfterSystemChange)
                             withObject:nil
                          waitUntilDone:NO];
    }
  }
}

- (void)handleRouteChange:(NSNotification*)notification {
  (void)notification;
  _state->route_changes.fetch_add(1U, std::memory_order_relaxed);
  if (_state->active.load(std::memory_order_acquire)) {
    _restartRequested = YES;
    [self performSelectorOnMainThread:@selector(restartAfterSystemChange)
                           withObject:nil
                        waitUntilDone:NO];
  }
}

- (void)handleMediaServicesReset:(NSNotification*)notification {
  (void)notification;
  _state->media_services_reset.store(true, std::memory_order_release);
  [self performSelectorOnMainThread:@selector(handleMediaServicesResetOnMain)
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)restartAfterSystemChange {
  if (!_restartRequested) return;
  _restartRequested = NO;
  [self stop];
  if ([self startWithError:nil]) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:MOLAppleAudioHostDidRestartNotification
                      object:self];
  }
}

- (void)handleMediaServicesResetOnMain {
  [self stop];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:MOLAppleAudioHostMediaServicesResetNotification
                    object:self];
}
#endif

@end
