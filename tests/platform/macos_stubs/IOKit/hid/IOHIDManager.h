// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_TESTS_MACOS_STUBS_IOKIT_HID_IOHIDMANAGER_H_
#define MOL_TESTS_MACOS_STUBS_IOKIT_HID_IOHIDMANAGER_H_

#include <stdint.h>

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* IOHIDManagerRef;
typedef void* IOHIDValueRef;
typedef void* IOHIDElementRef;
typedef int32_t IOReturn;
typedef uint32_t IOOptionBits;
typedef void (*IOHIDValueCallback)(void* context, IOReturn result, void* sender,
                                   IOHIDValueRef value);

#define kIOHIDDeviceUsagePageKey "DeviceUsagePage"
#define kIOHIDDeviceUsageKey "DeviceUsage"

enum {
  kIOHIDOptionsTypeNone = 0,
  kIOReturnSuccess = 0,
};

IOHIDManagerRef IOHIDManagerCreate(CFAllocatorRef allocator, IOOptionBits options);
void IOHIDManagerSetDeviceMatching(IOHIDManagerRef manager, CFMutableDictionaryRef matching);
IOReturn IOHIDManagerOpen(IOHIDManagerRef manager, IOOptionBits options);
IOReturn IOHIDManagerClose(IOHIDManagerRef manager, IOOptionBits options);
CFSetRef IOHIDManagerCopyDevices(IOHIDManagerRef manager);
void IOHIDManagerRegisterInputValueCallback(IOHIDManagerRef manager,
                                            IOHIDValueCallback callback, void* context);
void IOHIDManagerScheduleWithRunLoop(IOHIDManagerRef manager, CFRunLoopRef run_loop,
                                     const void* mode);
void IOHIDManagerUnscheduleFromRunLoop(IOHIDManagerRef manager, CFRunLoopRef run_loop,
                                       const void* mode);
IOHIDElementRef IOHIDValueGetElement(IOHIDValueRef value);
uint32_t IOHIDElementGetUsagePage(IOHIDElementRef element);
uint32_t IOHIDElementGetUsage(IOHIDElementRef element);
CFIndex IOHIDValueGetIntegerValue(IOHIDValueRef value);

#ifdef __cplusplus
}
#endif

#endif  // MOL_TESTS_MACOS_STUBS_IOKIT_HID_IOHIDMANAGER_H_
