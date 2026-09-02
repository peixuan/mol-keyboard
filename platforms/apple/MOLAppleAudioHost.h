// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MOLAppleAudioErrorDomain;
FOUNDATION_EXPORT NSNotificationName const MOLAppleAudioHostDidRestartNotification;
FOUNDATION_EXPORT NSNotificationName const MOLAppleAudioHostMediaServicesResetNotification;

typedef struct MOLAppleAudioStatus {
  uint32_t sampleRate;
  uint32_t maximumFramesPerSlice;
  uint64_t callbackCount;
  uint64_t renderedFrames;
  uint32_t renderFailures;
  uint32_t nonFiniteSamples;
  uint32_t routeChanges;
  uint32_t interruptions;
  int32_t lastOSStatus;
  BOOL active;
  BOOL mediaServicesReset;
} MOLAppleAudioStatus;

@interface MOLAppleAudioHost : NSObject

- (BOOL)startWithError:(NSError* _Nullable* _Nullable)error;
- (void)stop;
- (int32_t)noteOn:(uint8_t)note velocity:(float)velocity gestureId:(uint64_t)gestureId;
- (int32_t)noteOff:(uint8_t)note gestureId:(uint64_t)gestureId;
- (int32_t)submitCommandType:(uint32_t)commandType
                   gestureId:(uint64_t)gestureId
                    integer0:(int32_t)integer0
                    integer1:(int32_t)integer1
                    integer2:(int32_t)integer2
                    integer3:(int32_t)integer3
                     scalar0:(float)scalar0
                     scalar1:(float)scalar1;
- (NSArray<NSNumber*>*)pollEvents;
- (nullable NSData*)exportRecording;
- (int32_t)loadRecording:(NSData*)data;
- (MOLAppleAudioStatus)status;

@end

NS_ASSUME_NONNULL_END
