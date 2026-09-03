// SPDX-License-Identifier: Apache-2.0
#include "physical_input.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "midi_input.hpp"

namespace molkeyboardd {
namespace {

constexpr const char* kIdPrefix = "winmm-midi:";

std::string utf8_name(const wchar_t* value) {
  if (value == nullptr || value[0] == L'\0') return "Windows MIDI input";
  const int size = WideCharToMultiByte(CP_UTF8, 0u, value, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return "Windows MIDI input";
  std::string result(static_cast<std::size_t>(size), '\0');
  (void)WideCharToMultiByte(CP_UTF8, 0u, value, -1, result.data(), size, nullptr, nullptr);
  result.pop_back();
  return result;
}

std::string channel_suffix(std::uint8_t filter) {
  return filter == 0u ? "omni" : "ch" + std::to_string(filter);
}

std::string device_id(UINT index, std::uint8_t filter) {
  return std::string(kIdPrefix) + std::to_string(index) + ":" + channel_suffix(filter);
}

bool parse_device_id(const std::string& id, UINT& index, std::uint8_t& filter) {
  if (id.rfind(kIdPrefix, 0u) != 0u) return false;
  const std::size_t separator = id.find(':', std::char_traits<char>::length(kIdPrefix));
  if (separator == std::string::npos) return false;
  try {
    std::size_t consumed = 0u;
    const std::string index_text = id.substr(std::char_traits<char>::length(kIdPrefix),
                                             separator - std::char_traits<char>::length(kIdPrefix));
    const unsigned long parsed = std::stoul(index_text, &consumed, 10);
    if (consumed != index_text.size() || parsed > UINT_MAX) return false;
    index = static_cast<UINT>(parsed);
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

class WindowsMidiInput final : public PhysicalInputAdapter {
 public:
  ~WindowsMidiInput() override { detach(); }

  std::vector<molcontrol::DeviceInfo> devices() override {
    std::vector<molcontrol::DeviceInfo> result;
    const UINT count = midiInGetNumDevs();
    for (UINT index = 0u; index < count; ++index) {
      MIDIINCAPSW capabilities{};
      if (midiInGetDevCapsW(index, &capabilities, sizeof(capabilities)) != MMSYSERR_NOERROR) continue;
      const std::string name = utf8_name(capabilities.szPname);
      for (std::uint8_t filter = 0u; filter <= 16u; ++filter) {
        const std::string id = device_id(index, filter);
        result.push_back({id, name + " (" + (filter == 0u ? "Omni" : "Channel " +
                                                               std::to_string(filter)) +
                                       ")",
                          "windows-winmm-midi", result.empty(), active_id() == id, false, true,
                          true});
      }
    }
    return result;
  }

  mol_result_t attach(const std::string& id, CommandSink sink) override {
    UINT index = 0u;
    std::uint8_t filter = 0u;
    MIDIINCAPSW capabilities{};
    if (!sink || !parse_device_id(id, index, filter) || index >= midiInGetNumDevs() ||
        midiInGetDevCapsW(index, &capabilities, sizeof(capabilities)) != MMSYSERR_NOERROR)
      return MOL_ERROR_INVALID_ARGUMENT;
    detach();
    auto decoder = std::make_unique<MidiStreamDecoder>(
        UINT32_C(0x4d490000) | (static_cast<std::uint32_t>(index) + 1u), filter, std::move(sink));
    HMIDIIN handle = nullptr;
    const MMRESULT opened = midiInOpen(&handle, index, reinterpret_cast<DWORD_PTR>(&read_callback),
                                       reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
    if (opened != MMSYSERR_NOERROR) return MOL_ERROR_IO;
    {
      std::lock_guard<std::mutex> lock(decoder_mutex_);
      decoder_ = std::move(decoder);
    }
    handle_ = handle;
    active_ = id;
    accepting_.store(true, std::memory_order_release);
    if (midiInStart(handle_) != MMSYSERR_NOERROR) {
      detach();
      return MOL_ERROR_IO;
    }
    return MOL_OK;
  }

  void detach() override {
    accepting_.store(false, std::memory_order_release);
    if (handle_ != nullptr) {
      (void)midiInStop(handle_);
      (void)midiInReset(handle_);
      (void)midiInClose(handle_);
      handle_ = nullptr;
    }
    std::lock_guard<std::mutex> lock(decoder_mutex_);
    if (decoder_ != nullptr) decoder_->release_all();
    decoder_.reset();
    active_.clear();
  }

  std::string active_id() const override { return active_; }

 private:
  static void CALLBACK read_callback(HMIDIIN, UINT message, DWORD_PTR instance, DWORD_PTR packed,
                                     DWORD_PTR) {
    if (message != MIM_DATA && message != MIM_MOREDATA) return;
    // WinMM passes the application pointer and packed short message in pointer-sized fields.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    auto* self = reinterpret_cast<WindowsMidiInput*>(instance);
    if (self == nullptr || !self->accepting_.load(std::memory_order_acquire)) return;
    const std::uint8_t status = static_cast<std::uint8_t>(packed);
    if (status < 0x80u) return;
    std::uint8_t size = 1u;
    if (status < 0xf0u)
      size = static_cast<std::uint8_t>((status & 0xf0u) == 0xc0u ||
                                              (status & 0xf0u) == 0xd0u
                                          ? 2u
                                          : 3u);
    else if (status == 0xf1u || status == 0xf3u)
      size = 2u;
    else if (status == 0xf2u)
      size = 3u;
    const std::uint8_t bytes[3] = {status, static_cast<std::uint8_t>(packed >> 8u),
                                   static_cast<std::uint8_t>(packed >> 16u)};
    std::lock_guard<std::mutex> lock(self->decoder_mutex_);
    if (self->accepting_.load(std::memory_order_acquire) && self->decoder_ != nullptr)
      (void)self->decoder_->feed(bytes, size);
  }

  std::atomic<bool> accepting_{false};
  std::mutex decoder_mutex_;
  std::unique_ptr<MidiStreamDecoder> decoder_;
  HMIDIIN handle_ = nullptr;
  std::string active_;
};

}  // namespace

std::unique_ptr<PhysicalInputAdapter> make_midi_input_adapter() {
  return std::make_unique<WindowsMidiInput>();
}

}  // namespace molkeyboardd
