// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_TESTS_MACOS_STUBS_COREMIDI_COREMIDI_H_
#define MOL_TESTS_MACOS_STUBS_COREMIDI_COREMIDI_H_

#include <stdint.h>

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t MIDIClientRef;
typedef uint32_t MIDIEndpointRef;
typedef uint32_t MIDIPortRef;
typedef int32_t MIDIUniqueID;
typedef unsigned long ItemCount;
typedef int32_t OSStatus;
typedef uint32_t UInt32;
typedef uint64_t MIDITimeStamp;

typedef struct MIDIPacket {
  MIDITimeStamp timeStamp;
  uint16_t length;
  uint8_t data[256];
} MIDIPacket;

typedef struct MIDIPacketList {
  UInt32 numPackets;
  MIDIPacket packet[1];
} MIDIPacketList;

typedef void (*MIDIReadProc)(const MIDIPacketList* packet_list, void* context,
                             void* connection_context);

enum { noErr = 0 };

extern const CFStringRef kMIDIPropertyDisplayName;
extern const CFStringRef kMIDIPropertyUniqueID;

ItemCount MIDIGetNumberOfSources(void);
MIDIEndpointRef MIDIGetSource(ItemCount index);
OSStatus MIDIObjectGetStringProperty(uint32_t object, CFStringRef property, CFStringRef* value);
OSStatus MIDIObjectGetIntegerProperty(uint32_t object, CFStringRef property, int32_t* value);
OSStatus MIDIClientCreate(CFStringRef name, void* notify, void* context, MIDIClientRef* client);
OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef name, MIDIReadProc read_proc,
                             void* context, MIDIPortRef* port);
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source,
                               void* connection_context);
OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source);
OSStatus MIDIPortDispose(MIDIPortRef port);
OSStatus MIDIClientDispose(MIDIClientRef client);

static inline const MIDIPacket* MIDIPacketNext(const MIDIPacket* packet) {
  return packet + 1;
}

#ifdef __cplusplus
}
#endif

#endif  // MOL_TESTS_MACOS_STUBS_COREMIDI_COREMIDI_H_
