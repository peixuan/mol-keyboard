// SPDX-License-Identifier: Apache-2.0
#include "physical_input.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace molkeyboardd {
namespace {

constexpr const char* kAllKeyboardsId = "raw-input:all-keyboards";
constexpr const wchar_t* kWindowClass = L"MolKeyboardRawInputWindow";
constexpr UINT kStopMessage = WM_APP + 1u;
constexpr std::uint32_t kSourceId = UINT32_C(0x57494e31);

std::uint16_t usage_from_virtual_key(USHORT key) {
  struct Mapping {
    USHORT key;
    std::uint16_t usage;
  };
  static constexpr std::array<Mapping, 30u> mappings = {
      {{'Z', 0x001du}, {'S', 0x0016u}, {'X', 0x001bu}, {'D', 0x0007u}, {'C', 0x0006u},
       {'V', 0x0019u}, {'G', 0x000au}, {'B', 0x0005u}, {'H', 0x000bu}, {'N', 0x0011u},
       {'J', 0x000du}, {'M', 0x0010u}, {'Q', 0x0014u}, {'2', 0x001fu}, {'W', 0x001au},
       {'3', 0x0020u}, {'E', 0x0008u}, {'R', 0x0015u}, {'5', 0x0022u}, {'T', 0x0017u},
       {'6', 0x0023u}, {'Y', 0x001cu}, {'7', 0x0024u}, {'U', 0x0018u}, {'I', 0x000cu},
       {'9', 0x0026u}, {'O', 0x0012u}, {'0', 0x0027u}, {'P', 0x0013u}, {VK_OEM_4, 0x002fu}}};
  for (const Mapping& mapping : mappings)
    if (mapping.key == key) return mapping.usage;
  return 0u;
}

bool has_raw_keyboard() {
  UINT count = 0u;
  if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1))
    return false;
  std::vector<RAWINPUTDEVICELIST> devices(count);
  if (count != 0u && GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST)) ==
                         static_cast<UINT>(-1))
    return false;
  for (const RAWINPUTDEVICELIST& device : devices)
    if (device.dwType == RIM_TYPEKEYBOARD) return true;
  return false;
}

class WindowsRawInput final : public PhysicalInputAdapter {
 public:
  ~WindowsRawInput() override { detach(); }

  std::vector<molcontrol::DeviceInfo> devices() override {
    if (!has_raw_keyboard()) return {};
    return {{kAllKeyboardsId, "All accessible Raw Input keyboards", "windows-raw-input", true,
             active_id() == kAllKeyboardsId, false, true}};
  }

  mol_result_t attach(const std::string& id, CommandSink sink) override {
    if (id != kAllKeyboardsId || !sink || !has_raw_keyboard()) return MOL_ERROR_INVALID_ARGUMENT;
    detach();
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      sink_ = std::move(sink);
      ready_ = false;
      start_result_ = MOL_ERROR_INVALID_STATE;
    }
    stop_.store(false, std::memory_order_release);
    thread_ = std::thread(&WindowsRawInput::run, this);
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
    HWND window = nullptr;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      window = window_;
    }
    if (window != nullptr) (void)PostMessageW(window, kStopMessage, 0u, 0);
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(state_mutex_);
    window_ = nullptr;
    active_.clear();
    sink_ = {};
  }

  std::string active_id() const override {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return active_;
  }

 private:
  static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    // Win32 stores application pointers in pointer-sized integer message fields by contract.
    WindowsRawInput* self =
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        reinterpret_cast<WindowsRawInput*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      // NOLINTNEXTLINE(performance-no-int-to-ptr)
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      self = static_cast<WindowsRawInput*>(create->lpCreateParams);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self == nullptr) return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_INPUT) {
      // NOLINTNEXTLINE(performance-no-int-to-ptr)
      self->handle_input(reinterpret_cast<HRAWINPUT>(lparam));
      return 0;
    }
    if (message == kStopMessage) {
      DestroyWindow(window);
      return 0;
    }
    if (message == WM_DESTROY) {
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  void signal_ready(mol_result_t result, HWND window = nullptr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    start_result_ = result;
    window_ = window;
    if (result == MOL_OK) active_ = kAllKeyboardsId;
    ready_ = true;
    ready_condition_.notify_all();
  }

  void run() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = window_proc;
    window_class.lpszClassName = kWindowClass;
    if (RegisterClassExW(&window_class) == 0u && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      signal_ready(MOL_ERROR_IO);
      return;
    }
    const HWND window = CreateWindowExW(0u, kWindowClass, L"", 0u, 0, 0, 0, 0, HWND_MESSAGE,
                                        nullptr, instance, this);
    if (window == nullptr) {
      signal_ready(MOL_ERROR_IO);
      return;
    }
    RAWINPUTDEVICE registration{};
    registration.usUsagePage = 0x01u;
    registration.usUsage = 0x06u;
    registration.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    registration.hwndTarget = window;
    if (RegisterRawInputDevices(&registration, 1u, sizeof(registration)) == FALSE) {
      DestroyWindow(window);
      signal_ready(MOL_ERROR_IO);
      return;
    }
    signal_ready(MOL_OK, window);
    MSG message{};
    while (!stop_.load(std::memory_order_acquire) && GetMessageW(&message, nullptr, 0u, 0u) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    registration.dwFlags = RIDEV_REMOVE;
    registration.hwndTarget = nullptr;
    (void)RegisterRawInputDevices(&registration, 1u, sizeof(registration));
    release_all();
  }

  void handle_input(HRAWINPUT handle) {
    alignas(std::max_align_t) std::array<std::byte, 1024u> buffer{};
    UINT size = static_cast<UINT>(buffer.size());
    if (GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1) ||
        size > buffer.size())
      return;
    const auto* input = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (input->header.dwType != RIM_TYPEKEYBOARD) return;
    const USHORT key = input->data.keyboard.VKey;
    const std::uint16_t usage = usage_from_virtual_key(key);
    std::uint8_t note = 0u;
    if (usage == 0u || mol_keyboard_note_from_hid_usage(usage, &note) != MOL_OK || key >= 256u)
      return;
    const bool released = (input->data.keyboard.Flags & RI_KEY_BREAK) != 0u;
    if (!released && gestures_[key] == 0u) {
      mol_command_t command{};
      command.struct_size = static_cast<std::uint32_t>(sizeof(command));
      command.api_version = MOL_API_VERSION;
      command.command_type = MOL_COMMAND_NOTE_ON;
      command.source_id = kSourceId;
      command.target_frame = MOL_FRAME_IMMEDIATE;
      command.gesture_id = next_gesture_++;
      command.payload.note.note = note;
      command.payload.note.velocity = 0.8f;
      if (sink_(command) == MOL_OK) gestures_[key] = command.gesture_id;
    } else if (released && gestures_[key] != 0u) {
      mol_command_t command{};
      command.struct_size = static_cast<std::uint32_t>(sizeof(command));
      command.api_version = MOL_API_VERSION;
      command.command_type = MOL_COMMAND_NOTE_OFF;
      command.source_id = kSourceId;
      command.target_frame = MOL_FRAME_IMMEDIATE;
      command.gesture_id = gestures_[key];
      if (sink_(command) == MOL_OK) gestures_[key] = 0u;
    }
  }

  void release_all() {
    for (std::uint64_t& gesture : gestures_) {
      if (gesture == 0u) continue;
      mol_command_t command{};
      command.struct_size = static_cast<std::uint32_t>(sizeof(command));
      command.api_version = MOL_API_VERSION;
      command.command_type = MOL_COMMAND_NOTE_OFF;
      command.source_id = kSourceId;
      command.target_frame = MOL_FRAME_IMMEDIATE;
      command.gesture_id = gesture;
      (void)sink_(command);
      gesture = 0u;
    }
  }

  mutable std::mutex state_mutex_;
  std::condition_variable ready_condition_;
  std::thread thread_;
  CommandSink sink_;
  HWND window_ = nullptr;
  std::string active_;
  std::array<std::uint64_t, 256u> gestures_{};
  std::uint64_t next_gesture_ = UINT64_C(0x57494e0000000001);
  mol_result_t start_result_ = MOL_ERROR_INVALID_STATE;
  bool ready_ = false;
  std::atomic<bool> stop_{false};
};

}  // namespace

std::unique_ptr<PhysicalInputAdapter> make_physical_input_adapter() {
  return std::make_unique<WindowsRawInput>();
}

}  // namespace molkeyboardd
