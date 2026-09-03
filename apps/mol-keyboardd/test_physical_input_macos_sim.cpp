// SPDX-License-Identifier: Apache-2.0
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "physical_input.hpp"

namespace {

enum class FakeKind { generic, set };

struct FakeObject {
  explicit FakeObject(FakeKind object_kind = FakeKind::generic) : kind(object_kind) {}
  FakeKind kind;
};

struct FakeElement {
  std::uint32_t page;
  std::uint32_t usage;
};

struct FakeValue {
  FakeElement element;
  CFIndex value;
};

struct QueuedValue {
  std::uint32_t page;
  std::uint32_t usage;
  CFIndex value;
};

std::atomic<bool> keyboard_present{true};
std::mutex fake_mutex;
std::deque<QueuedValue> queued_values;
IOHIDValueCallback registered_callback = nullptr;
void* registered_context = nullptr;

[[noreturn]] void fail(const char* message) {
  std::fprintf(stderr, "macOS input simulation failure: %s\n", message);
  std::exit(1);
}

void require(bool condition, const char* message) {
  if (!condition) fail(message);
}

void queue_value(std::uint32_t page, std::uint32_t usage, CFIndex value) {
  std::lock_guard<std::mutex> lock(fake_mutex);
  queued_values.push_back({page, usage, value});
}

bool wait_for_commands(const std::vector<mol_command_t>& commands, const std::mutex& mutex,
                       std::size_t count) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    {
      std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));
      if (commands.size() >= count) return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

}  // namespace

extern "C" {

const CFAllocatorRef kCFAllocatorDefault = nullptr;
const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks{};
const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks{};
const CFNumberType kCFNumberIntType = 0;
const void* kCFRunLoopDefaultMode = nullptr;

CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef, CFIndex,
                                                 const CFDictionaryKeyCallBacks*,
                                                 const CFDictionaryValueCallBacks*) {
  return new FakeObject();
}

CFNumberRef CFNumberCreate(CFAllocatorRef, CFNumberType, const void*) {
  return new FakeObject();
}

void CFDictionarySetValue(CFMutableDictionaryRef, const void*, const void*) {}

void CFRelease(const void* value) { delete static_cast<const FakeObject*>(value); }

CFIndex CFSetGetCount(CFSetRef set) {
  const auto* object = static_cast<const FakeObject*>(set);
  return object != nullptr && object->kind == FakeKind::set && keyboard_present.load() ? 1 : 0;
}

CFRunLoopRef CFRunLoopGetCurrent() { return nullptr; }

CFRunLoopRunResult CFRunLoopRunInMode(const void*, CFTimeInterval, Boolean) {
  QueuedValue queued{};
  IOHIDValueCallback callback = nullptr;
  void* context = nullptr;
  {
    std::lock_guard<std::mutex> lock(fake_mutex);
    if (!queued_values.empty()) {
      queued = queued_values.front();
      queued_values.pop_front();
      callback = registered_callback;
      context = registered_context;
    }
  }
  if (callback != nullptr) {
    FakeValue value{{queued.page, queued.usage}, queued.value};
    callback(context, kIOReturnSuccess, nullptr, &value);
  } else {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return 0;
}

IOHIDManagerRef IOHIDManagerCreate(CFAllocatorRef, IOOptionBits) { return new FakeObject(); }

void IOHIDManagerSetDeviceMatching(IOHIDManagerRef, CFMutableDictionaryRef) {}

IOReturn IOHIDManagerOpen(IOHIDManagerRef, IOOptionBits) { return kIOReturnSuccess; }

IOReturn IOHIDManagerClose(IOHIDManagerRef, IOOptionBits) { return kIOReturnSuccess; }

CFSetRef IOHIDManagerCopyDevices(IOHIDManagerRef) { return new FakeObject(FakeKind::set); }

void IOHIDManagerRegisterInputValueCallback(IOHIDManagerRef, IOHIDValueCallback callback,
                                            void* context) {
  std::lock_guard<std::mutex> lock(fake_mutex);
  registered_callback = callback;
  registered_context = context;
}

void IOHIDManagerScheduleWithRunLoop(IOHIDManagerRef, CFRunLoopRef, const void*) {}

void IOHIDManagerUnscheduleFromRunLoop(IOHIDManagerRef, CFRunLoopRef, const void*) {
  std::lock_guard<std::mutex> lock(fake_mutex);
  registered_callback = nullptr;
  registered_context = nullptr;
}

IOHIDElementRef IOHIDValueGetElement(IOHIDValueRef value) {
  return &static_cast<FakeValue*>(value)->element;
}

std::uint32_t IOHIDElementGetUsagePage(IOHIDElementRef element) {
  return static_cast<FakeElement*>(element)->page;
}

std::uint32_t IOHIDElementGetUsage(IOHIDElementRef element) {
  return static_cast<FakeElement*>(element)->usage;
}

CFIndex IOHIDValueGetIntegerValue(IOHIDValueRef value) {
  return static_cast<FakeValue*>(value)->value;
}

}  // extern "C"

int main() {
  auto input = molkeyboardd::make_physical_input_adapter();
  require(input != nullptr, "adapter creation");

  keyboard_present.store(false);
  require(input->devices().empty(), "absent keyboard must not be advertised");
  keyboard_present.store(true);

  const auto devices = input->devices();
  require(devices.size() == 1u, "one aggregate IOHID keyboard device");
  require(devices[0].id == "iohid:all-keyboards", "stable IOHID device identifier");
  require(devices[0].backend == "macos-iohid", "IOHID backend name");
  require(input->attach("missing", [](const mol_command_t&) { return MOL_OK; }) ==
              MOL_ERROR_INVALID_ARGUMENT,
          "unknown device rejection");

  std::mutex commands_mutex;
  std::vector<mol_command_t> commands;
  const auto sink = [&](const mol_command_t& command) {
    std::lock_guard<std::mutex> lock(commands_mutex);
    commands.push_back(command);
    return MOL_OK;
  };
  require(input->attach(devices[0].id, sink) == MOL_OK, "IOHID attachment");
  require(input->active_id() == devices[0].id, "active IOHID identifier");

  queue_value(kHIDPage_GenericDesktop, 0x001du, 1);
  queue_value(kHIDPage_KeyboardOrKeypad, 0x001du, 1);
  queue_value(kHIDPage_KeyboardOrKeypad, 0x001du, 1);
  queue_value(kHIDPage_KeyboardOrKeypad, 0x001du, 0);
  require(wait_for_commands(commands, commands_mutex, 2u), "press and release delivery");

  std::uint64_t first_gesture = 0u;
  {
    std::lock_guard<std::mutex> lock(commands_mutex);
    require(commands.size() == 2u, "wrong-page and repeated presses must be ignored");
    require(commands[0].command_type == MOL_COMMAND_NOTE_ON, "first command note-on");
    require(commands[0].payload.note.note == 60u, "Z usage maps to middle C");
    require(std::fabs(commands[0].payload.note.velocity - 0.8f) < 0.0001f,
            "keyboard velocity");
    require(commands[1].command_type == MOL_COMMAND_NOTE_OFF, "second command note-off");
    require(commands[1].gesture_id == commands[0].gesture_id, "gesture ownership");
    first_gesture = commands[0].gesture_id;
  }
  require(first_gesture != 0u, "non-zero gesture identifier");

  queue_value(kHIDPage_KeyboardOrKeypad, 0x001bu, 1);
  require(wait_for_commands(commands, commands_mutex, 3u), "second note-on delivery");
  input->detach();
  {
    std::lock_guard<std::mutex> lock(commands_mutex);
    require(commands.size() == 4u, "detach must release the owned gesture");
    require(commands[2].command_type == MOL_COMMAND_NOTE_ON, "second key note-on");
    require(commands[3].command_type == MOL_COMMAND_NOTE_OFF, "detach note-off");
    require(commands[3].gesture_id == commands[2].gesture_id, "detach gesture ownership");
  }
  require(input->active_id().empty(), "detach clears active identifier");

  std::puts("macOS IOHID simulation passed");
  return 0;
}
