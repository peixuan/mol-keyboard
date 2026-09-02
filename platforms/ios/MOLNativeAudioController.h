// SPDX-License-Identifier: Apache-2.0
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MOLNativeAudioController : NSObject

- (NSString*)handleRequestText:(NSString*)requestText;
- (BOOL)submitHardwareNote:(uint8_t)note on:(BOOL)on gestureId:(uint64_t)gestureId;
- (BOOL)submitHardwareSustain:(BOOL)enabled gestureId:(uint64_t)gestureId;
- (void)applicationDidBecomeActive;
- (void)applicationWillResignActive;
- (void)applicationDidEnterBackground;
- (void)stopUserAudio;

@end

NS_ASSUME_NONNULL_END
