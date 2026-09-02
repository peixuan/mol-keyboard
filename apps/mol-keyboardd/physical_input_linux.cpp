// SPDX-License-Identifier: Apache-2.0
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "physical_input.hpp"

namespace molkeyboardd {
namespace {

constexpr std::uint32_t kSourceId = UINT32_C(0x4c4e5831);

bool bit_set(const unsigned long* bits, unsigned int bit) {
  constexpr unsigned int kBitsPerLong = sizeof(unsigned long) * 8u;
  return (bits[bit / kBitsPerLong] & (1ul << (bit % kBitsPerLong))) != 0u;
}

std::uint16_t usage_from_key(unsigned int key) {
  struct Mapping {
    unsigned int key;
    std::uint16_t usage;
  };
  static constexpr std::array<Mapping, 30u> mappings = {
      {{KEY_Z, 0x001du}, {KEY_S, 0x0016u},        {KEY_X, 0x001bu}, {KEY_D, 0x0007u},
       {KEY_C, 0x0006u}, {KEY_V, 0x0019u},        {KEY_G, 0x000au}, {KEY_B, 0x0005u},
       {KEY_H, 0x000bu}, {KEY_N, 0x0011u},        {KEY_J, 0x000du}, {KEY_M, 0x0010u},
       {KEY_Q, 0x0014u}, {KEY_2, 0x001fu},        {KEY_W, 0x001au}, {KEY_3, 0x0020u},
       {KEY_E, 0x0008u}, {KEY_R, 0x0015u},        {KEY_5, 0x0022u}, {KEY_T, 0x0017u},
       {KEY_6, 0x0023u}, {KEY_Y, 0x001cu},        {KEY_7, 0x0024u}, {KEY_U, 0x0018u},
       {KEY_I, 0x000cu}, {KEY_9, 0x0026u},        {KEY_O, 0x0012u}, {KEY_0, 0x0027u},
       {KEY_P, 0x0013u}, {KEY_LEFTBRACE, 0x002fu}}};
  for (const Mapping& mapping : mappings)
    if (mapping.key == key) return mapping.usage;
  return 0u;
}

bool keyboard_descriptor(int descriptor) {
  constexpr std::size_t kWords =
      (EV_MAX + sizeof(unsigned long) * 8u) / (sizeof(unsigned long) * 8u);
  std::array<unsigned long, kWords> event_bits{};
  return ioctl(descriptor, EVIOCGBIT(0, sizeof(event_bits)), event_bits.data()) >= 0 &&
         bit_set(event_bits.data(), EV_KEY);
}

class LinuxEvdevInput final : public PhysicalInputAdapter {
 public:
  ~LinuxEvdevInput() override { detach(); }

  std::vector<molcontrol::DeviceInfo> devices() override {
    std::vector<molcontrol::DeviceInfo> result;
    std::error_code error;
    const std::filesystem::path directory("/dev/input");
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error)) {
      const std::string file_name = iterator->path().filename().string();
      if (file_name.rfind("event", 0u) != 0u) continue;
      const std::string path = iterator->path().string();
      const int descriptor = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (descriptor < 0) continue;
      const bool keyboard = keyboard_descriptor(descriptor);
      std::array<char, 256u> name{};
      if (keyboard) (void)ioctl(descriptor, EVIOCGNAME(name.size()), name.data());
      (void)::close(descriptor);
      if (!keyboard) continue;
      result.push_back({path, name[0] == '\0' ? file_name : name.data(), "linux-evdev",
                        result.empty(), active_id() == path, false, true});
    }
    return result;
  }

  mol_result_t attach(const std::string& id, CommandSink sink) override {
    if (!sink) return MOL_ERROR_INVALID_ARGUMENT;
    bool known = false;
    for (const molcontrol::DeviceInfo& device : devices()) known = known || device.id == id;
    if (!known) return MOL_ERROR_INVALID_ARGUMENT;
    detach();
    const int descriptor = ::open(id.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0 || !keyboard_descriptor(descriptor)) {
      if (descriptor >= 0) (void)::close(descriptor);
      return MOL_ERROR_IO;
    }
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      descriptor_ = descriptor;
      active_ = id;
      sink_ = std::move(sink);
    }
    stop_.store(false, std::memory_order_release);
    thread_ = std::thread(&LinuxEvdevInput::run, this);
    return MOL_OK;
  }

  void detach() override {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (descriptor_ >= 0) (void)::close(descriptor_);
    descriptor_ = -1;
    active_.clear();
    sink_ = {};
  }

  std::string active_id() const override {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return active_;
  }

 private:
  void run() {
    std::array<input_event, 32u> events{};
    while (!stop_.load(std::memory_order_acquire)) {
      pollfd ready{descriptor_, POLLIN, 0};
      const int result = ::poll(&ready, 1u, 100);
      if (result < 0) break;
      if (result == 0) continue;
      if ((ready.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) break;
      if ((ready.revents & POLLIN) == 0) continue;
      const ssize_t bytes = ::read(descriptor_, events.data(), sizeof(events));
      if (bytes <= 0) continue;
      const std::size_t count = static_cast<std::size_t>(bytes) / sizeof(input_event);
      for (std::size_t index = 0u; index < count; ++index) handle_event(events[index]);
    }
    release_all();
  }

  void handle_event(const input_event& event) {
    if (event.type != EV_KEY || event.value == 2 || event.code > KEY_MAX) return;
    const std::uint16_t usage = usage_from_key(event.code);
    std::uint8_t note = 0u;
    if (usage == 0u || mol_keyboard_note_from_hid_usage(usage, &note) != MOL_OK) return;
    if (event.value == 1 && gestures_[event.code] == 0u) {
      mol_command_t command{};
      command.struct_size = static_cast<std::uint32_t>(sizeof(command));
      command.api_version = MOL_API_VERSION;
      command.command_type = MOL_COMMAND_NOTE_ON;
      command.source_id = kSourceId;
      command.target_frame = MOL_FRAME_IMMEDIATE;
      command.gesture_id = next_gesture_++;
      command.payload.note.note = note;
      command.payload.note.velocity = 0.8f;
      if (sink_(command) == MOL_OK) gestures_[event.code] = command.gesture_id;
    } else if (event.value == 0 && gestures_[event.code] != 0u) {
      send_note_off(gestures_[event.code]);
      gestures_[event.code] = 0u;
    }
  }

  void send_note_off(std::uint64_t gesture) {
    mol_command_t command{};
    command.struct_size = static_cast<std::uint32_t>(sizeof(command));
    command.api_version = MOL_API_VERSION;
    command.command_type = MOL_COMMAND_NOTE_OFF;
    command.source_id = kSourceId;
    command.target_frame = MOL_FRAME_IMMEDIATE;
    command.gesture_id = gesture;
    (void)sink_(command);
  }

  void release_all() {
    for (std::uint64_t& gesture : gestures_) {
      if (gesture != 0u) send_note_off(gesture);
      gesture = 0u;
    }
  }

  mutable std::mutex state_mutex_;
  std::thread thread_;
  CommandSink sink_;
  std::string active_;
  int descriptor_ = -1;
  std::atomic<bool> stop_{false};
  std::array<std::uint64_t, KEY_MAX + 1u> gestures_{};
  std::uint64_t next_gesture_ = UINT64_C(0x4c4e580000000001);
};

}  // namespace

std::unique_ptr<PhysicalInputAdapter> make_physical_input_adapter() {
  return std::make_unique<LinuxEvdevInput>();
}

}  // namespace molkeyboardd
