// SPDX-License-Identifier: Apache-2.0
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "midi_input.hpp"
#include "physical_input.hpp"

namespace molkeyboardd {
namespace {

constexpr const char* kIdPrefix = "coremidi:";

std::string cf_string(CFStringRef value) {
  if (value == nullptr) return "macOS MIDI input";
  const CFIndex length = CFStringGetLength(value);
  const CFIndex capacity = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  if (capacity <= 1) return "macOS MIDI input";
  std::vector<char> bytes(static_cast<std::size_t>(capacity));
  if (!CFStringGetCString(value, bytes.data(), capacity, kCFStringEncodingUTF8))
    return "macOS MIDI input";
  return bytes.data();
}

std::string endpoint_name(MIDIEndpointRef endpoint) {
  CFStringRef name = nullptr;
  if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) != noErr ||
      name == nullptr)
    return "macOS MIDI input";
  const std::string result = cf_string(name);
  CFRelease(name);
  return result;
}

bool endpoint_unique_id(MIDIEndpointRef endpoint, MIDIUniqueID& id) {
  return MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &id) == noErr;
}

std::string channel_suffix(std::uint8_t filter) {
  return filter == 0u ? "omni" : "ch" + std::to_string(filter);
}

std::string device_id(MIDIUniqueID unique_id, std::uint8_t filter) {
  return std::string(kIdPrefix) + std::to_string(unique_id) + ":" + channel_suffix(filter);
}

bool parse_device_id(const std::string& id, MIDIUniqueID& unique_id, std::uint8_t& filter) {
  if (id.rfind(kIdPrefix, 0u) != 0u) return false;
  const std::size_t separator = id.rfind(':');
  if (separator <= std::char_traits<char>::length(kIdPrefix)) return false;
  try {
    std::size_t consumed = 0u;
    const std::string unique_text = id.substr(std::char_traits<char>::length(kIdPrefix),
                                              separator - std::char_traits<char>::length(kIdPrefix));
    const long parsed = std::stol(unique_text, &consumed, 10);
    if (consumed != unique_text.size() || parsed < INT32_MIN || parsed > INT32_MAX) return false;
    unique_id = static_cast<MIDIUniqueID>(parsed);
  } catch (...) {
    return false;
  }
  const std::string suffix = id.substr(separator + 1u);
  if (suffix == "omni") {
    filter = 0u;
    return true;
  }
  if (suffix.rfind("ch", 0u) != 0u) return false;
  try {
    std::size_t consumed = 0u;
    const unsigned long parsed = std::stoul(suffix.substr(2u), &consumed, 10);
    if (consumed != suffix.size() - 2u || parsed < 1u || parsed > 16u) return false;
    filter = static_cast<std::uint8_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

MIDIEndpointRef find_endpoint(MIDIUniqueID unique_id) {
  const ItemCount count = MIDIGetNumberOfSources();
  for (ItemCount index = 0u; index < count; ++index) {
    const MIDIEndpointRef endpoint = MIDIGetSource(index);
    MIDIUniqueID candidate = 0;
    if (endpoint != 0u && endpoint_unique_id(endpoint, candidate) && candidate == unique_id)
      return endpoint;
  }
  return 0u;
}

class MacMidiInput final : public PhysicalInputAdapter {
 public:
  ~MacMidiInput() override { detach(); }

  std::vector<molcontrol::DeviceInfo> devices() override {
    std::vector<molcontrol::DeviceInfo> result;
    const ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount index = 0u; index < count; ++index) {
      const MIDIEndpointRef endpoint = MIDIGetSource(index);
      MIDIUniqueID unique_id = 0;
      if (endpoint == 0u || !endpoint_unique_id(endpoint, unique_id)) continue;
      const std::string name = endpoint_name(endpoint);
      for (std::uint8_t filter = 0u; filter <= 16u; ++filter) {
        const std::string id = device_id(unique_id, filter);
        result.push_back({id, name + " (" + (filter == 0u ? "Omni" : "Channel " +
                                                               std::to_string(filter)) +
                                       ")",
                          "macos-coremidi", result.empty(), active_id() == id, false, true, true});
      }
    }
    return result;
  }

  mol_result_t attach(const std::string& id, CommandSink sink) override {
    MIDIUniqueID unique_id = 0;
    std::uint8_t filter = 0u;
    if (!sink || !parse_device_id(id, unique_id, filter)) return MOL_ERROR_INVALID_ARGUMENT;
    const MIDIEndpointRef endpoint = find_endpoint(unique_id);
    if (endpoint == 0u) return MOL_ERROR_INVALID_ARGUMENT;
    detach();
    MIDIClientRef client = 0u;
    MIDIPortRef port = 0u;
    if (MIDIClientCreate(CFSTR("MoL Keyboard MIDI"), nullptr, nullptr, &client) != noErr)
      return MOL_ERROR_IO;
    if (MIDIInputPortCreate(client, CFSTR("MoL Keyboard Input"), read_callback, this, &port) !=
        noErr) {
      (void)MIDIClientDispose(client);
      return MOL_ERROR_IO;
    }
    auto decoder = std::make_unique<MidiStreamDecoder>(
        static_cast<std::uint32_t>(unique_id) ^ UINT32_C(0x4d490000), filter, std::move(sink));
    {
      std::lock_guard<std::mutex> lock(decoder_mutex_);
      decoder_ = std::move(decoder);
    }
    client_ = client;
    port_ = port;
    endpoint_ = endpoint;
    active_ = id;
    accepting_.store(true, std::memory_order_release);
    if (MIDIPortConnectSource(port_, endpoint_, this) != noErr) {
      detach();
      return MOL_ERROR_IO;
    }
    return MOL_OK;
  }

  void detach() override {
    accepting_.store(false, std::memory_order_release);
    if (port_ != 0u && endpoint_ != 0u) (void)MIDIPortDisconnectSource(port_, endpoint_);
    if (port_ != 0u) (void)MIDIPortDispose(port_);
    if (client_ != 0u) (void)MIDIClientDispose(client_);
    port_ = 0u;
    client_ = 0u;
    endpoint_ = 0u;
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    if (decoder_ != nullptr) decoder_->release_all();
    decoder_.reset();
    active_.clear();
  }

  std::string active_id() const override { return active_; }

 private:
  static void read_callback(const MIDIPacketList* packet_list, void* context, void*) {
    auto* self = static_cast<MacMidiInput*>(context);
    if (self == nullptr || packet_list == nullptr ||
        !self->accepting_.load(std::memory_order_acquire))
      return;
    std::lock_guard<std::mutex> lock(self->decoder_mutex_);
    if (!self->accepting_.load(std::memory_order_acquire) || self->decoder_ == nullptr) return;
    const MIDIPacket* packet = &packet_list->packet[0];
    for (UInt32 index = 0u; index < packet_list->numPackets; ++index) {
      (void)self->decoder_->feed(packet->data, packet->length);
      packet = MIDIPacketNext(packet);
    }
  }

  std::atomic<bool> accepting_{false};
  std::mutex decoder_mutex_;
  std::unique_ptr<MidiStreamDecoder> decoder_;
  MIDIClientRef client_ = 0u;
  MIDIPortRef port_ = 0u;
  MIDIEndpointRef endpoint_ = 0u;
  std::string active_;
};

}  // namespace

std::unique_ptr<PhysicalInputAdapter> make_midi_input_adapter() {
  return std::make_unique<MacMidiInput>();
}

}  // namespace molkeyboardd
