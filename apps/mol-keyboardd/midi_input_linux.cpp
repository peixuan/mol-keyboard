// SPDX-License-Identifier: Apache-2.0
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "midi_input.hpp"
#include "physical_input.hpp"

namespace molkeyboardd {
namespace {

constexpr const char* kIdPrefix = "linux-midi:";

std::string channel_suffix(std::uint8_t filter) {
  return filter == 0u ? "omni" : "ch" + std::to_string(filter);
}

std::string device_id(const std::filesystem::path& path, std::uint8_t filter) {
  return std::string(kIdPrefix) + path.string() + ":" + channel_suffix(filter);
}

bool raw_midi_name(const std::string& name) {
  if (name.rfind("midiC", 0u) != 0u) return false;
  const std::size_t device = name.find('D', 5u);
  if (device == std::string::npos || device == 5u || device + 1u == name.size()) return false;
  for (std::size_t index = 5u; index < device; ++index)
    if (name[index] < '0' || name[index] > '9') return false;
  for (std::size_t index = device + 1u; index < name.size(); ++index)
    if (name[index] < '0' || name[index] > '9') return false;
  return true;
}

bool parse_device_id(const std::string& id, std::filesystem::path& path, std::uint8_t& filter) {
  if (id.rfind(kIdPrefix, 0u) != 0u) return false;
  const std::size_t separator = id.rfind(':');
  if (separator <= std::char_traits<char>::length(kIdPrefix)) return false;
  path = id.substr(std::char_traits<char>::length(kIdPrefix),
                   separator - std::char_traits<char>::length(kIdPrefix));
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

std::uint32_t source_id(const std::string& path) {
  std::uint32_t hash = UINT32_C(2166136261);
  for (const unsigned char byte : path) hash = (hash ^ byte) * UINT32_C(16777619);
  return hash == 0u ? 1u : hash;
}

class LinuxMidiInput final : public PhysicalInputAdapter {
 public:
  explicit LinuxMidiInput(std::filesystem::path directory) : directory_(std::move(directory)) {}
  ~LinuxMidiInput() override { detach(); }

  std::vector<molcontrol::DeviceInfo> devices() override {
    std::vector<molcontrol::DeviceInfo> result;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(directory_, error), end;
         !error && iterator != end; iterator.increment(error)) {
      const std::string file_name = iterator->path().filename().string();
      if (!raw_midi_name(file_name)) continue;
      const int descriptor = ::open(iterator->path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (descriptor < 0) continue;
      (void)::close(descriptor);
      for (std::uint8_t filter = 0u; filter <= 16u; ++filter) {
        const std::string id = device_id(iterator->path(), filter);
        result.push_back({id, file_name + " (" + (filter == 0u ? "Omni" : "Channel " +
                                                                   std::to_string(filter)) +
                                       ")",
                          "linux-raw-midi", result.empty(), active_id() == id, false, true, true});
      }
    }
    return result;
  }

  mol_result_t attach(const std::string& id, CommandSink sink) override {
    std::filesystem::path path;
    std::uint8_t filter = 0u;
    if (!sink || !parse_device_id(id, path, filter) || path.parent_path() != directory_ ||
        !raw_midi_name(path.filename().string()))
      return MOL_ERROR_INVALID_ARGUMENT;
    detach();
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) return MOL_ERROR_IO;
    descriptor_ = descriptor;
    decoder_ = std::make_unique<MidiStreamDecoder>(source_id(path.string()), filter, std::move(sink));
    active_ = id;
    stop_.store(false, std::memory_order_release);
    thread_ = std::thread(&LinuxMidiInput::run, this);
    return MOL_OK;
  }

  void detach() override {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    if (decoder_ != nullptr) decoder_->release_all();
    decoder_.reset();
    if (descriptor_ >= 0) (void)::close(descriptor_);
    descriptor_ = -1;
    active_.clear();
  }

  std::string active_id() const override { return active_; }

 private:
  void run() {
    std::array<std::uint8_t, 256u> bytes{};
    while (!stop_.load(std::memory_order_acquire)) {
      pollfd ready{descriptor_, POLLIN, 0};
      const int result = ::poll(&ready, 1u, 100);
      if (result < 0) break;
      if (result == 0) continue;
      if ((ready.revents & (POLLERR | POLLNVAL)) != 0) break;
      if ((ready.revents & POLLIN) != 0) {
        const ssize_t count = ::read(descriptor_, bytes.data(), bytes.size());
        if (count > 0) (void)decoder_->feed(bytes.data(), static_cast<std::size_t>(count));
      }
      if ((ready.revents & POLLHUP) != 0 && (ready.revents & POLLIN) == 0) break;
    }
  }

  std::filesystem::path directory_;
  std::thread thread_;
  std::unique_ptr<MidiStreamDecoder> decoder_;
  std::string active_;
  int descriptor_ = -1;
  std::atomic<bool> stop_{false};
};

}  // namespace

std::unique_ptr<PhysicalInputAdapter> make_midi_input_adapter() {
  return std::make_unique<LinuxMidiInput>("/dev/snd");
}

std::unique_ptr<PhysicalInputAdapter> make_linux_midi_input_adapter_for_test(
    const std::filesystem::path& directory) {
  return std::make_unique<LinuxMidiInput>(directory);
}

}  // namespace molkeyboardd
