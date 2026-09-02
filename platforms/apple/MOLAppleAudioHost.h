// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const MOLAppleAudioErrorDomain;

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
- (MOLAppleAudioStatus)status;

@end

NS_ASSUME_NONNULL_END
