// SPDX-License-Identifier: Apache-2.0
#import "MOLNativeAudioController.h"

#import <UIKit/UIKit.h>

#import "MOLAppleAudioHost.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr NSUInteger kBridgeVersion = 1U;
constexpr NSUInteger kMaximumRequestCharacters = 2800000U;
constexpr NSUInteger kMaximumResponseCharacters = 2800000U;
constexpr NSUInteger kMaximumRecordingBytes = 2U * 1024U * 1024U;
constexpr NSUInteger kMaximumBase64Characters = 2796204U;
constexpr NSUInteger kMaximumErrorCharacters = 256U;
constexpr NSUInteger kEventFieldCount = 5U;
constexpr NSUInteger kMaximumPendingEventFields = 256U * kEventFieldCount;
constexpr std::uint64_t kMaximumSafeJavaScriptInteger = 9007199254740991ULL;
constexpr NSTimeInterval kEventPumpInterval = 0.25;

enum CommandType : std::uint32_t {
  kCommandNoteOn = 1U,
  kCommandNoteOff = 2U,
  kCommandSustain = 5U,
  kCommandAllNotesOff = 6U,
  kCommandAllSoundOff = 7U,
  kCommandSetMasterGain = 8U,
  kCommandSetPreset = 9U,
  kCommandSetParameter = 10U,
  kCommandSetOctave = 11U,
  kCommandSetTranspose = 12U,
  kCommandSetScale = 13U,
  kCommandSetChord = 14U,
  kCommandSetArpeggiator = 15U,
  kCommandSetTempo = 16U,
  kCommandSetTimeSignature = 17U,
  kCommandTransportStart = 18U,
  kCommandTransportStop = 19U,
  kCommandRecordStart = 21U,
  kCommandRecordStop = 22U,
  kCommandPlaybackStart = 23U,
  kCommandPlaybackStop = 24U,
  kCommandResetEngine = 26U,
  kCommandSetMetronome = 27U,
  kCommandSetPortamento = 28U,
};

enum EventType : std::uint32_t {
  kEventRecordingChanged = 7U,
  kEventPlaybackChanged = 14U,
};

BOOL is_json_number(id value) {
  if (![value isKindOfClass:NSNumber.class]) return NO;
  return CFGetTypeID((__bridge CFTypeRef)value) != CFBooleanGetTypeID();
}

BOOL read_integer(NSDictionary<NSString*, id>* dictionary, NSString* key, std::int64_t minimum,
                  std::int64_t maximum, std::int64_t* output) {
  id value = dictionary[key];
  if (!is_json_number(value)) return NO;
  const double number = [value doubleValue];
  if (!std::isfinite(number) || std::trunc(number) != number ||
      number < static_cast<double>(minimum) || number > static_cast<double>(maximum)) {
    return NO;
  }
  *output = static_cast<std::int64_t>(number);
  return YES;
}

BOOL read_scalar(NSDictionary<NSString*, id>* dictionary, NSString* key, float* output) {
  id value = dictionary[key];
  if (!is_json_number(value)) return NO;
  const double number = [value doubleValue];
  if (!std::isfinite(number) ||
      std::abs(number) > static_cast<double>(std::numeric_limits<float>::max())) {
    return NO;
  }
  *output = static_cast<float>(number);
  return YES;
}

BOOL has_exact_keys(NSDictionary<NSString*, id>* dictionary, NSArray<NSString*>* expected) {
  if (dictionary.count != expected.count) return NO;
  NSSet<NSString*>* actual = [NSSet setWithArray:dictionary.allKeys];
  return [actual isEqualToSet:[NSSet setWithArray:expected]];
}

BOOL is_allowed_command(std::uint32_t command) {
  switch (command) {
    case kCommandNoteOn:
    case kCommandNoteOff:
    case kCommandSustain:
    case kCommandAllNotesOff:
    case kCommandAllSoundOff:
    case kCommandSetMasterGain:
    case kCommandSetPreset:
    case kCommandSetParameter:
    case kCommandSetOctave:
    case kCommandSetTranspose:
    case kCommandSetScale:
    case kCommandSetChord:
    case kCommandSetArpeggiator:
    case kCommandSetTempo:
    case kCommandSetTimeSignature:
    case kCommandTransportStart:
    case kCommandTransportStop:
    case kCommandRecordStart:
    case kCommandRecordStop:
    case kCommandPlaybackStart:
    case kCommandPlaybackStop:
    case kCommandResetEngine:
    case kCommandSetMetronome:
    case kCommandSetPortamento:
      return YES;
    default:
      return NO;
  }
}

BOOL is_persistent_command(std::uint32_t command) {
  switch (command) {
    case kCommandSetMasterGain:
    case kCommandSetPreset:
    case kCommandSetParameter:
    case kCommandSetOctave:
    case kCommandSetTranspose:
    case kCommandSetScale:
    case kCommandSetChord:
    case kCommandSetArpeggiator:
    case kCommandSetTempo:
    case kCommandSetTimeSignature:
    case kCommandSetMetronome:
    case kCommandSetPortamento:
      return YES;
    default:
      return NO;
  }
}

NSString* serialize_response(NSDictionary<NSString*, id>* response) {
  NSError* error = nil;
  NSData* data = [NSJSONSerialization dataWithJSONObject:response options:0 error:&error];
  if (data == nil || data.length > kMaximumResponseCharacters) {
    return @"{\"ok\":false,\"error\":\"Native response serialization failed\"}";
  }
  NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  return text ?: @"{\"ok\":false,\"error\":\"Native response encoding failed\"}";
}

NSString* success_response(NSDictionary<NSString*, id>* fields) {
  NSMutableDictionary<NSString*, id>* response = [fields mutableCopy];
  response[@"ok"] = @YES;
  return serialize_response(response);
}

NSString* failure_response(NSString* message) {
  NSString* bounded = message.length <= kMaximumErrorCharacters
                          ? message
                          : [message substringToIndex:kMaximumErrorCharacters];
  return serialize_response(@{@"ok" : @NO, @"error" : bounded});
}

}  // namespace

@interface MOLNativeAudioController ()

- (void)pumpTimer:(NSTimer*)timer;
- (void)hostDidRestart:(NSNotification*)notification;
- (void)hostMediaServicesReset:(NSNotification*)notification;
- (NSString*)submitBridgeCommand:(NSDictionary<NSString*, id>*)params;
- (BOOL)startUserAudio:(NSError**)error;
- (void)rememberCommandType:(std::uint32_t)type
                  gestureId:(std::uint64_t)gestureId
                   integers:(const std::int64_t[4])integers
                    scalars:(const float[2])scalars;
- (void)updateStateForCommand:(std::uint32_t)type integer0:(std::int64_t)integer0;
- (void)restoreRuntimeState;
- (void)pumpNativeEvents;
- (BOOL)allowsBackgroundContinuation;
- (nullable NSURL*)createSequenceURL;
- (void)loadPersistedSequence;
- (void)persistLoadedSequence;
- (void)removePersistedSequence;

@end

@implementation MOLNativeAudioController {
  MOLAppleAudioHost* _host;
  NSTimer* _eventTimer;
  NSMutableArray<NSNumber*>* _pendingEvents;
  NSMutableDictionary<NSString*, NSArray<NSNumber*>*>* _replayControls;
  NSData* _loadedSequence;
  NSURL* _sequenceURL;
  BOOL _userStarted;
  BOOL _uiForeground;
  BOOL _transportRunning;
  BOOL _playbackRunning;
  BOOL _metronomeEnabled;
  std::uint64_t _routeRevision;
  std::int32_t _lastStartStatus;
}

- (instancetype)init {
  self = [super init];
  if (self == nil) return nil;
  _host = [[MOLAppleAudioHost alloc] init];
  if (_host == nil) return nil;
  _pendingEvents = [[NSMutableArray alloc] initWithCapacity:kMaximumPendingEventFields];
  _replayControls = [[NSMutableDictionary alloc] init];
  _uiForeground = YES;
  _sequenceURL = [self createSequenceURL];
  [self loadPersistedSequence];
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  [center addObserver:self
             selector:@selector(hostDidRestart:)
                 name:MOLAppleAudioHostDidRestartNotification
               object:_host];
  [center addObserver:self
             selector:@selector(hostMediaServicesReset:)
                 name:MOLAppleAudioHostMediaServicesResetNotification
               object:_host];
  __weak MOLNativeAudioController* weakSelf = self;
  _eventTimer = [NSTimer timerWithTimeInterval:kEventPumpInterval
                                       repeats:YES
                                         block:^(NSTimer* timer) {
                                           [weakSelf pumpTimer:timer];
                                         }];
  [NSRunLoop.mainRunLoop addTimer:_eventTimer forMode:NSRunLoopCommonModes];
  return self;
}

- (void)dealloc {
  [_eventTimer invalidate];
  [NSNotificationCenter.defaultCenter removeObserver:self];
  [_host stop];
}

- (NSString*)handleRequestText:(NSString*)requestText {
  if (requestText.length < 2U || requestText.length > kMaximumRequestCharacters) {
    return failure_response(@"Request size is invalid");
  }
  NSData* requestData = [requestText dataUsingEncoding:NSUTF8StringEncoding];
  if (requestData == nil || requestData.length > kMaximumRequestCharacters * 4U) {
    return failure_response(@"Request encoding is invalid");
  }
  NSError* parseError = nil;
  id parsed = [NSJSONSerialization JSONObjectWithData:requestData options:0 error:&parseError];
  if (![parsed isKindOfClass:NSDictionary.class]) {
    return failure_response(parseError.localizedDescription ?: @"Request JSON is invalid");
  }
  NSDictionary<NSString*, id>* request = parsed;
  if (!has_exact_keys(request, @[ @"version", @"method", @"params" ])) {
    return failure_response(@"Unexpected or missing request fields");
  }
  std::int64_t version = 0;
  if (!read_integer(request, @"version", kBridgeVersion, kBridgeVersion, &version)) {
    return failure_response(@"Unsupported bridge version");
  }
  id methodValue = request[@"method"];
  id paramsValue = request[@"params"];
  if (![methodValue isKindOfClass:NSString.class] || [methodValue length] == 0U ||
      [methodValue length] > 64U || ![paramsValue isKindOfClass:NSDictionary.class]) {
    return failure_response(@"Request method or parameters are invalid");
  }
  NSString* method = methodValue;
  NSDictionary<NSString*, id>* params = paramsValue;

  if ([method isEqualToString:@"runtime.start"]) {
    if (!has_exact_keys(params, @[])) return failure_response(@"Unexpected parameters");
    NSError* error = nil;
    if (![self startUserAudio:&error]) {
      return failure_response(error.localizedDescription ?: @"Native audio could not start");
    }
    return success_response(@{});
  }
  if ([method isEqualToString:@"runtime.stop"]) {
    if (!has_exact_keys(params, @[])) return failure_response(@"Unexpected parameters");
    [self stopUserAudio];
    return success_response(@{});
  }
  if ([method isEqualToString:@"runtime.status"]) {
    if (!has_exact_keys(params, @[])) return failure_response(@"Unexpected parameters");
    MOLAppleAudioStatus status = _host.status;
    const std::uint64_t revision =
        std::max(_routeRevision, static_cast<std::uint64_t>(status.routeChanges));
    return success_response(@{
      @"active" : @(status.active),
      @"userStarted" : @(_userStarted),
      @"sampleRate" : @(status.sampleRate),
      @"framesPerBurst" : @(status.maximumFramesPerSlice),
      @"audioApi" : @1,
      @"callbackCount" : @(status.callbackCount),
      @"renderedFrames" : @(status.renderedFrames),
      @"renderFailures" : @(status.renderFailures),
      @"nonFiniteSamples" : @(status.nonFiniteSamples),
      @"lastError" :
          @(status.active || _lastStartStatus == 0 ? status.lastOSStatus : _lastStartStatus),
      @"disconnected" : @NO,
      @"routeRevision" : @(revision),
      @"interruptions" : @(status.interruptions),
      @"mediaServicesReset" : @(status.mediaServicesReset),
    });
  }
  if ([method isEqualToString:@"command.submit"]) {
    return [self submitBridgeCommand:params];
  }
  if ([method isEqualToString:@"events.poll"]) {
    if (!has_exact_keys(params, @[])) return failure_response(@"Unexpected parameters");
    [self pumpNativeEvents];
    NSArray<NSNumber*>* events = [_pendingEvents copy];
    [_pendingEvents removeAllObjects];
    return success_response(@{@"events" : events});
  }
  if ([method isEqualToString:@"recording.export"]) {
    if (!has_exact_keys(params, @[])) return failure_response(@"Unexpected parameters");
    NSData* sequence = [_host exportRecording] ?: _loadedSequence;
    if (sequence.length == 0U || sequence.length > kMaximumRecordingBytes) {
      return failure_response(@"No complete recording is available");
    }
    _loadedSequence = [sequence copy];
    [self persistLoadedSequence];
    return success_response(@{@"base64" : [_loadedSequence base64EncodedStringWithOptions:0]});
  }
  if ([method isEqualToString:@"recording.load"]) {
    if (!has_exact_keys(params, @[ @"base64" ])) {
      return failure_response(@"Unexpected recording parameters");
    }
    id encodedValue = params[@"base64"];
    if (![encodedValue isKindOfClass:NSString.class] || [encodedValue length] == 0U ||
        [encodedValue length] > kMaximumBase64Characters) {
      return failure_response(@"Recording payload is invalid");
    }
    NSData* sequence = [[NSData alloc] initWithBase64EncodedString:encodedValue options:0];
    if (sequence.length == 0U || sequence.length > kMaximumRecordingBytes) {
      return failure_response(@"Recording size is invalid");
    }
    const int32_t result = [_host loadRecording:sequence];
    if (result == 0) {
      _loadedSequence = [sequence copy];
      [self persistLoadedSequence];
    }
    return serialize_response(@{@"ok" : @(result == 0), @"result" : @(result)});
  }
  return failure_response(@"Method is not allowed");
}

- (BOOL)submitHardwareNote:(uint8_t)note on:(BOOL)on gestureId:(uint64_t)gestureId {
  if (!_uiForeground || !_userStarted || !_host.status.active) return NO;
  const int32_t result = [_host submitCommandType:on ? kCommandNoteOn : kCommandNoteOff
                                        gestureId:gestureId
                                         integer0:note
                                         integer1:0
                                         integer2:0
                                         integer3:0
                                          scalar0:on ? 0.82F : 0.0F
                                          scalar1:0.0F];
  return result == 0;
}

- (BOOL)submitHardwareSustain:(BOOL)enabled gestureId:(uint64_t)gestureId {
  if (!_uiForeground || !_userStarted || !_host.status.active) return NO;
  return [_host submitCommandType:kCommandSustain
                        gestureId:gestureId
                         integer0:0
                         integer1:0
                         integer2:0
                         integer3:0
                          scalar0:enabled ? 1.0F : 0.0F
                          scalar1:0.0F] == 0;
}

- (void)applicationDidBecomeActive {
  _uiForeground = YES;
}

- (void)applicationWillResignActive {
  _uiForeground = NO;
  if (_host.status.active) {
    (void)[_host submitCommandType:kCommandAllNotesOff
                         gestureId:0U
                          integer0:0
                          integer1:0
                          integer2:0
                          integer3:0
                           scalar0:0.0F
                           scalar1:0.0F];
  }
}

- (void)applicationDidEnterBackground {
  _uiForeground = NO;
  if (_host.status.active) {
    (void)[_host submitCommandType:kCommandAllNotesOff
                         gestureId:0U
                          integer0:0
                          integer1:0
                          integer2:0
                          integer3:0
                           scalar0:0.0F
                           scalar1:0.0F];
  }
  if (![self allowsBackgroundContinuation]) [self stopUserAudio];
}

- (void)stopUserAudio {
  if (_host.status.active) {
    (void)[_host submitCommandType:kCommandAllSoundOff
                         gestureId:0U
                          integer0:0
                          integer1:0
                          integer2:0
                          integer3:0
                           scalar0:0.0F
                           scalar1:0.0F];
  }
  [_host stop];
  _userStarted = NO;
  _transportRunning = NO;
  _playbackRunning = NO;
  _metronomeEnabled = NO;
  [_pendingEvents removeAllObjects];
}

- (NSString*)submitBridgeCommand:(NSDictionary<NSString*, id>*)params {
  NSArray<NSString*>* expected = @[ @"type", @"gesture", @"i0", @"i1", @"i2", @"i3", @"f0", @"f1" ];
  if (!has_exact_keys(params, expected)) {
    return failure_response(@"Unexpected or missing command fields");
  }
  std::int64_t typeValue = 0;
  std::int64_t gestureValue = 0;
  std::int64_t integers[4]{};
  float scalars[2]{};
  if (!read_integer(params, @"type", 0, std::numeric_limits<std::uint32_t>::max(), &typeValue) ||
      !is_allowed_command(static_cast<std::uint32_t>(typeValue)) ||
      !read_integer(params, @"gesture", 0, static_cast<std::int64_t>(kMaximumSafeJavaScriptInteger),
                    &gestureValue)) {
    return failure_response(@"Command type or gesture is invalid");
  }
  NSArray<NSString*>* integerKeys = @[ @"i0", @"i1", @"i2", @"i3" ];
  for (NSUInteger index = 0U; index < integerKeys.count; ++index) {
    std::int64_t value = 0;
    if (!read_integer(params, integerKeys[index], std::numeric_limits<std::int32_t>::min(),
                      std::numeric_limits<std::int32_t>::max(), &value)) {
      return failure_response(@"Command integer is invalid");
    }
    integers[index] = value;
  }
  if (!read_scalar(params, @"f0", &scalars[0]) || !read_scalar(params, @"f1", &scalars[1])) {
    return failure_response(@"Command scalar is invalid");
  }

  const std::uint32_t type = static_cast<std::uint32_t>(typeValue);
  const int32_t result = [_host submitCommandType:type
                                        gestureId:static_cast<std::uint64_t>(gestureValue)
                                         integer0:static_cast<std::int32_t>(integers[0])
                                         integer1:static_cast<std::int32_t>(integers[1])
                                         integer2:static_cast<std::int32_t>(integers[2])
                                         integer3:static_cast<std::int32_t>(integers[3])
                                          scalar0:scalars[0]
                                          scalar1:scalars[1]];
  if (result == 0) {
    [self rememberCommandType:type
                    gestureId:static_cast<std::uint64_t>(gestureValue)
                     integers:integers
                      scalars:scalars];
    [self updateStateForCommand:type integer0:integers[0]];
  }
  return serialize_response(@{@"ok" : @(result == 0), @"result" : @(result)});
}

- (BOOL)startUserAudio:(NSError**)error {
  if (_userStarted && _host.status.active) return YES;
  if (![_host startWithError:error]) {
    _lastStartStatus = error != nullptr && *error != nil ? (*error).code : -1;
    _userStarted = NO;
    return NO;
  }
  _lastStartStatus = 0;
  _userStarted = YES;
  [self restoreRuntimeState];
  return YES;
}

- (void)rememberCommandType:(std::uint32_t)type
                  gestureId:(std::uint64_t)gestureId
                   integers:(const std::int64_t[4])integers
                    scalars:(const float[2])scalars {
  if (!is_persistent_command(type)) return;
  NSString* key = type == kCommandSetParameter
                      ? [NSString stringWithFormat:@"%u:%lld", type, integers[0]]
                      : [NSString stringWithFormat:@"%u", type];
  _replayControls[key] = @[
    @(type), @(gestureId), @(integers[0]), @(integers[1]), @(integers[2]), @(integers[3]),
    @(scalars[0]), @(scalars[1])
  ];
}

- (void)updateStateForCommand:(std::uint32_t)type integer0:(std::int64_t)integer0 {
  switch (type) {
    case kCommandTransportStart:
      _transportRunning = YES;
      break;
    case kCommandTransportStop:
      _transportRunning = NO;
      break;
    case kCommandPlaybackStart: {
      NSData* sequence = [_host exportRecording];
      if (sequence.length > 0U && sequence.length <= kMaximumRecordingBytes) {
        _loadedSequence = [sequence copy];
        [self persistLoadedSequence];
      }
      _playbackRunning = YES;
      break;
    }
    case kCommandPlaybackStop:
      _playbackRunning = NO;
      break;
    case kCommandSetMetronome:
      _metronomeEnabled = integer0 != 0;
      break;
    case kCommandResetEngine:
      _transportRunning = NO;
      _playbackRunning = NO;
      _metronomeEnabled = NO;
      _loadedSequence = nil;
      [_replayControls removeAllObjects];
      [self removePersistedSequence];
      break;
    default:
      break;
  }
  if (!_uiForeground && ![self allowsBackgroundContinuation]) [self stopUserAudio];
}

- (void)restoreRuntimeState {
  if (_loadedSequence != nil && [_host loadRecording:_loadedSequence] != 0) {
    _loadedSequence = nil;
    [self removePersistedSequence];
  }
  for (NSArray<NSNumber*>* command in _replayControls.allValues) {
    if (command.count != 8U) continue;
    (void)[_host submitCommandType:command[0].unsignedIntValue
                         gestureId:command[1].unsignedLongLongValue
                          integer0:command[2].intValue
                          integer1:command[3].intValue
                          integer2:command[4].intValue
                          integer3:command[5].intValue
                           scalar0:command[6].floatValue
                           scalar1:command[7].floatValue];
  }
  if (_playbackRunning) {
    (void)[_host submitCommandType:kCommandPlaybackStart
                         gestureId:0U
                          integer0:0
                          integer1:0
                          integer2:0
                          integer3:0
                           scalar0:0.0F
                           scalar1:0.0F];
  }
  if (_transportRunning) {
    (void)[_host submitCommandType:kCommandTransportStart
                         gestureId:0U
                          integer0:0
                          integer1:0
                          integer2:0
                          integer3:0
                           scalar0:0.0F
                           scalar1:0.0F];
  }
}

- (void)pumpTimer:(NSTimer*)timer {
  (void)timer;
  [self pumpNativeEvents];
}

- (void)pumpNativeEvents {
  if (!_host.status.active) return;
  NSArray<NSNumber*>* fields = [_host pollEvents];
  for (NSUInteger offset = 0U; offset + kEventFieldCount <= fields.count;
       offset += kEventFieldCount) {
    const std::uint32_t type = fields[offset].unsignedIntValue;
    const std::uint64_t detail = fields[offset + 4U].unsignedLongLongValue;
    if (type == kEventRecordingChanged && detail == 0U) {
      NSData* sequence = [_host exportRecording];
      if (sequence.length > 0U && sequence.length <= kMaximumRecordingBytes) {
        _loadedSequence = [sequence copy];
        [self persistLoadedSequence];
      }
    } else if (type == kEventPlaybackChanged) {
      _playbackRunning = detail != 0U;
    }
    while (_pendingEvents.count + kEventFieldCount > kMaximumPendingEventFields) {
      [_pendingEvents removeObjectsInRange:NSMakeRange(0U, kEventFieldCount)];
    }
    [_pendingEvents
        addObjectsFromArray:[fields subarrayWithRange:NSMakeRange(offset, kEventFieldCount)]];
  }
  if (!_uiForeground && ![self allowsBackgroundContinuation]) [self stopUserAudio];
}

- (BOOL)allowsBackgroundContinuation {
  return _playbackRunning || (_metronomeEnabled && _transportRunning);
}

- (void)hostDidRestart:(NSNotification*)notification {
  if (notification.object != _host || !_userStarted) return;
  _routeRevision += 1U;
  [self restoreRuntimeState];
}

- (void)hostMediaServicesReset:(NSNotification*)notification {
  if (notification.object != _host || !_userStarted) return;
  _routeRevision += 1U;
  if (_uiForeground || [self allowsBackgroundContinuation]) {
    NSError* error = nil;
    if ([_host startWithError:&error]) {
      [self restoreRuntimeState];
      return;
    }
    _lastStartStatus = error.code;
  }
  [self stopUserAudio];
}

- (NSURL*)createSequenceURL {
  NSFileManager* manager = NSFileManager.defaultManager;
  NSURL* support = [manager URLsForDirectory:NSApplicationSupportDirectory
                                   inDomains:NSUserDomainMask]
                       .firstObject;
  if (support == nil) return nil;
  NSURL* directory = [support URLByAppendingPathComponent:@"MoLKeyboard" isDirectory:YES];
  if (![manager createDirectoryAtURL:directory
          withIntermediateDirectories:YES
                           attributes:nil
                                error:nil]) {
    return nil;
  }
  return [directory URLByAppendingPathComponent:@"last-recording.molseq" isDirectory:NO];
}

- (void)loadPersistedSequence {
  if (_sequenceURL == nil) return;
  NSData* sequence = [NSData dataWithContentsOfURL:_sequenceURL
                                           options:NSDataReadingMappedIfSafe
                                             error:nil];
  if (sequence.length > 0U && sequence.length <= kMaximumRecordingBytes) {
    _loadedSequence = [sequence copy];
  } else if (sequence != nil) {
    [self removePersistedSequence];
  }
}

- (void)persistLoadedSequence {
  if (_sequenceURL == nil || _loadedSequence.length == 0U ||
      _loadedSequence.length > kMaximumRecordingBytes) {
    return;
  }
  (void)[_loadedSequence writeToURL:_sequenceURL options:NSDataWritingAtomic error:nil];
}

- (void)removePersistedSequence {
  if (_sequenceURL != nil) {
    (void)[NSFileManager.defaultManager removeItemAtURL:_sequenceURL error:nil];
  }
}

@end
