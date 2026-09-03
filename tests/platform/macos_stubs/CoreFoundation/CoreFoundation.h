// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_TESTS_MACOS_STUBS_COREFOUNDATION_COREFOUNDATION_H_
#define MOL_TESTS_MACOS_STUBS_COREFOUNDATION_COREFOUNDATION_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const void* CFAllocatorRef;
typedef void* CFMutableDictionaryRef;
typedef void* CFNumberRef;
typedef void* CFSetRef;
typedef void* CFRunLoopRef;
typedef const void* CFStringRef;
typedef long CFIndex;
typedef double CFTimeInterval;
typedef unsigned char Boolean;
typedef int CFNumberType;
typedef int CFRunLoopRunResult;
typedef unsigned int CFStringEncoding;

typedef struct CFDictionaryKeyCallBacks {
  int unused;
} CFDictionaryKeyCallBacks;

typedef struct CFDictionaryValueCallBacks {
  int unused;
} CFDictionaryValueCallBacks;

extern const CFAllocatorRef kCFAllocatorDefault;
extern const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
extern const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;
extern const CFNumberType kCFNumberIntType;
extern const void* kCFRunLoopDefaultMode;

#define CFSTR(value) ((const void*)(value))

CFMutableDictionaryRef CFDictionaryCreateMutable(
    CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks* key_callbacks,
    const CFDictionaryValueCallBacks* value_callbacks);
CFNumberRef CFNumberCreate(CFAllocatorRef allocator, CFNumberType type, const void* value);
void CFDictionarySetValue(CFMutableDictionaryRef dictionary, const void* key, const void* value);
void CFRelease(const void* value);
CFIndex CFSetGetCount(CFSetRef set);
CFRunLoopRef CFRunLoopGetCurrent(void);
CFRunLoopRunResult CFRunLoopRunInMode(const void* mode, CFTimeInterval seconds,
                                     Boolean return_after_source_handled);
CFIndex CFStringGetLength(CFStringRef string);
CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length, CFStringEncoding encoding);
Boolean CFStringGetCString(CFStringRef string, char* buffer, CFIndex capacity,
                           CFStringEncoding encoding);

enum { kCFStringEncodingUTF8 = 0x08000100u };

#ifdef __cplusplus
}
#endif

#endif  // MOL_TESTS_MACOS_STUBS_COREFOUNDATION_COREFOUNDATION_H_
