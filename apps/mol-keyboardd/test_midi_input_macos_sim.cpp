// SPDX-License-Identifier: Apache-2.0
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "physical_input.hpp"

namespace {

struct FakeString {
  std::string value;
};

MIDIReadProc registered_callback = nullptr;
void* registered_context = nullptr;
bool endpoint_present = true;

[[noreturn]] void fail(const char* message) {
  std::fprintf(stderr, "macOS MIDI simulation failure: %s\n", message);
  std::exit(1);
}

void require(bool condition, const char* message) {
  if (!condition) fail(message);
}

}  // namespace

extern "C" {

const CFStringRef kMIDIPropertyDisplayName = CFSTR("display-name");
const CFStringRef kMIDIPropertyUniqueID = CFSTR("unique-id");

CFIndex CFStringGetLength(CFStringRef string) {
  return static_cast<CFIndex>(static_cast<const FakeString*>(string)->value.size());
}

CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length, CFStringEncoding) {
  return length * 4;
}

Boolean CFStringGetCString(CFStringRef string, char* buffer, CFIndex capacity,
                           CFStringEncoding) {
  const std::string& value = static_cast<const FakeString*>(string)->value;
  if (capacity <= static_cast<CFIndex>(value.size())) return 0u;
  std::memcpy(buffer, value.c_str(), value.size() + 1u);
  return 1u;
}

void CFRelease(const void* value) { delete static_cast<const FakeString*>(value); }

ItemCount MIDIGetNumberOfSources() { return endpoint_present ? 1u : 0u; }

MIDIEndpointRef MIDIGetSource(ItemCount index) {
  return endpoint_present && index == 0u ? 23u : 0u;
}

OSStatus MIDIObjectGetStringProperty(uint32_t object, CFStringRef property, CFStringRef* value) {
  if (object != 23u || property != kMIDIPropertyDisplayName || value == nullptr) return -1;
  *value = new FakeString{"Simulated CoreMIDI Keyboard"};
  return noErr;
}

OSStatus MIDIObjectGetIntegerProperty(uint32_t object, CFStringRef property, int32_t* value) {
  if (object != 23u || property != kMIDIPropertyUniqueID || value == nullptr) return -1;
  *value = 4242;
  return noErr;
}

OSStatus MIDIClientCreate(CFStringRef, void*, void*, MIDIClientRef* client) {
  if (client == nullptr) return -1;
  *client = 31u;
  return noErr;
}

OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef, MIDIReadProc read_proc,
                             void* context, MIDIPortRef* port) {
  if (client != 31u || read_proc == nullptr || port == nullptr) return -1;
  registered_callback = read_proc;
  registered_context = context;
  *port = 37u;
  return noErr;
}

OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source, void*) {
  return port == 37u && source == 23u ? noErr : -1;
}

OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source) {
  return port == 37u && source == 23u ? noErr : -1;
}

OSStatus MIDIPortDispose(MIDIPortRef port) {
  registered_callback = nullptr;
  registered_context = nullptr;
  return port == 37u ? noErr : -1;
}

OSStatus MIDIClientDispose(MIDIClientRef client) { return client == 31u ? noErr : -1; }

}  // extern "C"

int main() {
  auto input = molkeyboardd::make_midi_input_adapter();
  endpoint_present = false;
  require(input->devices().empty(), "absent endpoint is not advertised");
  endpoint_present = true;
  const std::vector<molcontrol::DeviceInfo> devices = input->devices();
  require(devices.size() == 17u, "omni and sixteen channel-filtered endpoints");
  const auto selected = std::find_if(devices.begin(), devices.end(), [](const auto& device) {
    return device.id.size() >= 4u && device.id.compare(device.id.size() - 4u, 4u, ":ch3") == 0;
  });
  require(selected != devices.end() && selected->is_midi_input &&
              selected->backend == "macos-coremidi",
          "CoreMIDI endpoint metadata");

  std::mutex mutex;
  std::vector<mol_command_t> commands;
  require(input->attach(selected->id, [&](const mol_command_t& command) {
            std::lock_guard<std::mutex> lock(mutex);
            commands.push_back(command);
            return MOL_OK;
          }) == MOL_OK,
          "CoreMIDI endpoint attachment");
  require(registered_callback != nullptr, "CoreMIDI read callback registration");
  MIDIPacketList packets{};
  packets.numPackets = 1u;
  const std::uint8_t bytes[] = {0x91u, 50u, 100u, 0x92u, 60u, 127u, 0xb2u,
                                64u,   127u,      0xe2u, 0u,   64u,  0x82u, 60u, 0u};
  packets.packet[0].length = static_cast<std::uint16_t>(sizeof(bytes));
  std::copy(bytes, bytes + sizeof(bytes), packets.packet[0].data);
  registered_callback(&packets, registered_context, nullptr);
  {
    std::lock_guard<std::mutex> lock(mutex);
    require(commands.size() == 4u, "channel filter and MIDI command count");
    require(commands[0].command_type == MOL_COMMAND_NOTE_ON &&
                commands[1].command_type == MOL_COMMAND_SUSTAIN &&
                commands[2].command_type == MOL_COMMAND_PITCH_BEND &&
                commands[3].command_type == MOL_COMMAND_NOTE_OFF &&
                commands[3].gesture_id == commands[0].gesture_id,
            "CoreMIDI byte stream translation");
  }
  input->detach();
  require(input->active_id().empty() && registered_callback == nullptr,
          "CoreMIDI callback disposal");
  return 0;
}
