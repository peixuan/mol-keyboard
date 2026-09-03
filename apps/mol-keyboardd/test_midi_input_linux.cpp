// SPDX-License-Identifier: Apache-2.0
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "midi_input_linux_test.hpp"

namespace {

struct Cleanup {
  std::filesystem::path path;
  ~Cleanup() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};

bool wait_for_commands(const std::vector<mol_command_t>& commands, std::mutex& mutex,
                       std::size_t expected) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (commands.size() >= expected) return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

}  // namespace

int main() {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() /
      ("mol-midi-adapter-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path fifo = directory / "midiC7D3";
  Cleanup cleanup{directory};
  std::filesystem::create_directories(directory);
  if (::mkfifo(fifo.c_str(), 0600) != 0) return 1;
  const int writer = ::open(fifo.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (writer < 0) return 1;

  auto input = molkeyboardd::make_linux_midi_input_adapter_for_test(directory);
  const std::vector<molcontrol::DeviceInfo> devices = input->devices();
  if (devices.size() != 17u) return 1;
  std::string channel_two;
  for (const molcontrol::DeviceInfo& device : devices) {
    if (!device.is_midi_input || device.backend != "linux-raw-midi") return 1;
    if (device.id.size() >= 4u && device.id.compare(device.id.size() - 4u, 4u, ":ch2") == 0)
      channel_two = device.id;
  }
  if (channel_two.empty()) return 1;

  std::mutex mutex;
  std::vector<mol_command_t> commands;
  const mol_result_t attached = input->attach(channel_two, [&](const mol_command_t& command) {
    std::lock_guard<std::mutex> lock(mutex);
    commands.push_back(command);
    return MOL_OK;
  });
  if (attached != MOL_OK || input->active_id() != channel_two) return 1;
  const std::uint8_t stream[] = {0x90u, 40u, 127u, 0x91u, 60u, 100u, 0xb1u, 1u, 64u,
                                 64u,   127u,      0xe1u, 0u, 64u,  0x81u, 60u, 0u};
  if (::write(writer, stream, sizeof(stream)) != static_cast<ssize_t>(sizeof(stream)) ||
      !wait_for_commands(commands, mutex, 5u))
    return 1;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (commands.size() != 5u || commands[0].command_type != MOL_COMMAND_NOTE_ON ||
        commands[0].payload.note.note != 60u ||
        commands[1].command_type != MOL_COMMAND_SET_PARAMETER ||
        commands[1].payload.parameter.parameter != MOL_PARAMETER_MODULATION ||
        commands[2].command_type != MOL_COMMAND_SUSTAIN ||
        commands[3].command_type != MOL_COMMAND_PITCH_BEND ||
        commands[3].payload.scalar.value != 0.0f ||
        commands[4].command_type != MOL_COMMAND_NOTE_OFF ||
        commands[4].gesture_id != commands[0].gesture_id)
      return 1;
  }
  input->detach();
  if (!input->active_id().empty() || !wait_for_commands(commands, mutex, 8u)) return 1;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (commands[5].command_type != MOL_COMMAND_SUSTAIN ||
        commands[6].command_type != MOL_COMMAND_PITCH_BEND ||
        commands[7].command_type != MOL_COMMAND_SET_PARAMETER)
      return 1;
  }
  (void)::close(writer);
  return 0;
}
