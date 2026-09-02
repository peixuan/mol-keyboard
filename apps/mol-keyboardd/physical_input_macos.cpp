// SPDX-License-Identifier: Apache-2.0
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "physical_input.hpp"

namespace molkeyboardd {
namespace {

constexpr const char* kAllKeyboardsId = "iohid:all-keyboards";
constexpr std::uint32_t kSourceId = UINT32_C(0x4d414331);

CFMutableDictionaryRef keyboard_matching_dictionary() {
  CFMutableDictionaryRef result = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  if (result == nullptr) return nullptr;
  int page = kHIDPage_GenericDesktop;
  int usage = kHIDUsage_GD_Keyboard;
  CFNumberRef page_number =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, static_cast<const void*>(&page));
  CFNumberRef usage_number =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, static_cast<const void*>(&usage));
  if (page_number == nullptr || usage_number == nullptr) {
    if (page_number != nullptr) CFRelease(page_number);
    if (usage_number != nullptr) CFRelease(usage_number);
    CFRelease(result);
    return nullptr;
  }
  CFDictionarySetValue(result, CFSTR(kIOHIDDeviceUsagePageKey), page_number);
  CFDictionarySetValue(result, CFSTR(kIOHIDDeviceUsageKey), usage_number);
  CFRelease(page_number);
  CFRelease(usage_number);
  return result;
}

bool has_keyboard() {
  IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
  if (manager == nullptr) return false;
  CFMutableDictionaryRef matching = keyboard_matching_dictionary();
  if (matching == nullptr) {
    CFRelease(manager);
    return false;
  }
  IOHIDManagerSetDeviceMatching(manager, matching);
  CFRelease(matching);
  bool found = false;
  if (IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) == kIOReturnSuccess) {
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    found = devices != nullptr && CFSetGetCount(devices) != 0;
    if (devices != nullptr) CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
  }
  CFRelease(manager);
  return found;
}

class MacHidInput final : public PhysicalInputAdapter {
 public:
  ~MacHidInput() override { detach(); }

  std::vector<molcontrol::DeviceInfo> devices() override {
    if (!has_keyboard()) return {};
    return {{kAllKeyboardsId, "All accessible IOHID keyboards", "macos-iohid", true,
             active_id() == kAllKeyboardsId, false, true}};
  }

  mol_result_t attach(const std::string& id, CommandSink sink) override {
    if (id != kAllKeyboardsId || !sink || !has_keyboard()) return MOL_ERROR_INVALID_ARGUMENT;
    detach();
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      sink_ = std::move(sink);
      ready_ = false;
      start_result_ = MOL_ERROR_INVALID_STATE;
    }
    stop_.store(false, std::memory_order_release);
    thread_ = std::thread(&MacHidInput::run, this);
    std::unique_lock<std::mutex> lock(state_mutex_);
    if (!ready_condition_.wait_for(lock, std::chrono::seconds(2), [&] { return ready_; })) {
      lock.unlock();
      detach();
      return MOL_ERROR_IO;
    }
    const mol_result_t result = start_result_;
    lock.unlock();
    if (result != MOL_OK) detach();
    return result;
  }

  void detach() override {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(state_mutex_);
    active_.clear();
    sink_ = {};
  }

  std::string active_id() const override {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return active_;
  }

 private:
  static void input_callback(void* context, IOReturn result, void* sender, IOHIDValueRef value) {
    (void)sender;
    if (result != kIOReturnSuccess || context == nullptr || value == nullptr) return;
    static_cast<MacHidInput*>(context)->handle_value(value);
  }

  void signal_ready(mol_result_t result) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    start_result_ = result;
    if (result == MOL_OK) active_ = kAllKeyboardsId;
    ready_ = true;
    ready_condition_.notify_all();
  }

  void run() {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef matching = keyboard_matching_dictionary();
    if (manager == nullptr || matching == nullptr) {
      if (manager != nullptr) CFRelease(manager);
      if (matching != nullptr) CFRelease(matching);
      signal_ready(MOL_ERROR_IO);
      return;
    }
    IOHIDManagerSetDeviceMatching(manager, matching);
    CFRelease(matching);
    IOHIDManagerRegisterInputValueCallback(manager, input_callback, this);
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    if (IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
      IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
      CFRelease(manager);
      signal_ready(MOL_ERROR_IO);
      return;
    }
    signal_ready(MOL_OK);
    while (!stop_.load(std::memory_order_acquire))
      (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    release_all();
    IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
  }

  void handle_value(IOHIDValueRef value) {
    IOHIDElementRef element = IOHIDValueGetElement(value);
    if (element == nullptr || IOHIDElementGetUsagePage(element) != kHIDPage_KeyboardOrKeypad)
      return;
    const std::uint32_t usage = IOHIDElementGetUsage(element);
    if (usage >= gestures_.size()) return;
    std::uint8_t note = 0u;
    if (mol_keyboard_note_from_hid_usage(static_cast<std::uint16_t>(usage), &note) != MOL_OK)
      return;
    const bool pressed = IOHIDValueGetIntegerValue(value) != 0;
    if (pressed && gestures_[usage] == 0u) {
      mol_command_t command{};
      command.struct_size = static_cast<std::uint32_t>(sizeof(command));
      command.api_version = MOL_API_VERSION;
      command.command_type = MOL_COMMAND_NOTE_ON;
      command.source_id = kSourceId;
      command.target_frame = MOL_FRAME_IMMEDIATE;
      command.gesture_id = next_gesture_++;
      command.payload.note.note = note;
      command.payload.note.velocity = 0.8f;
      if (sink_(command) == MOL_OK) gestures_[usage] = command.gesture_id;
    } else if (!pressed && gestures_[usage] != 0u) {
      send_note_off(gestures_[usage]);
      gestures_[usage] = 0u;
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
  std::condition_variable ready_condition_;
  std::thread thread_;
  CommandSink sink_;
  std::string active_;
  std::atomic<bool> stop_{false};
  std::array<std::uint64_t, 256u> gestures_{};
  std::uint64_t next_gesture_ = UINT64_C(0x4d41430000000001);
  mol_result_t start_result_ = MOL_ERROR_INVALID_STATE;
  bool ready_ = false;
};

}  // namespace

std::unique_ptr<PhysicalInputAdapter> make_physical_input_adapter() {
  return std::make_unique<MacHidInput>();
}

}  // namespace molkeyboardd
