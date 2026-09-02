// SPDX-License-Identifier: Apache-2.0
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "audio_runtime.hpp"

namespace {

mol_command_t command(mol_command_type_t type) {
  mol_command_t result{};
  result.struct_size = static_cast<std::uint32_t>(sizeof(result));
  result.api_version = MOL_API_VERSION;
  result.command_type = type;
  result.target_frame = MOL_FRAME_IMMEDIATE;
  return result;
}

}  // namespace

int main() {
  molkeyboardd::AudioRuntime runtime;
  std::string warning;
  if (!runtime.start(true, "default", warning)) {
    std::fprintf(stderr, "Null audio runtime startup failed: %s\n", warning.c_str());
    return 1;
  }

  mol_command_t note_on = command(MOL_COMMAND_NOTE_ON);
  note_on.gesture_id = 99u;
  note_on.payload.note.note = 60u;
  note_on.payload.note.velocity = 0.8f;
  if (runtime.submit(note_on) != MOL_OK) return 1;
  std::this_thread::sleep_for(std::chrono::milliseconds(40));

  mol_engine_state_t state{};
  state.struct_size = static_cast<std::uint32_t>(sizeof(state));
  if (runtime.snapshot(state) != MOL_OK || state.sample_rate == 0u || state.channel_count != 2u ||
      state.current_frame == 0u) {
    std::fprintf(stderr, "Runtime state snapshot failed\n");
    return 1;
  }
  std::array<mol_event_t, 64u> events{};
  const std::size_t event_count = runtime.poll_events(events.data(), events.size());
  bool received_note_started = false;
  for (std::size_t index = 0u; index < event_count; ++index) {
    received_note_started =
        received_note_started ||
        (events[index].event_type == MOL_EVENT_NOTE_STARTED && events[index].gesture_id == 99u &&
         events[index].payload[MOL_EVENT_PAYLOAD_NOTE] == 60u);
  }
  if (!received_note_started) {
    std::fprintf(stderr, "Runtime event transport did not receive note start\n");
    return 1;
  }

  mol_command_t record_start = command(MOL_COMMAND_RECORD_START);
  mol_command_t record_stop = command(MOL_COMMAND_RECORD_STOP);
  mol_command_t note_off = command(MOL_COMMAND_NOTE_OFF);
  note_off.gesture_id = 99u;
  if (runtime.submit(record_start) != MOL_OK || runtime.submit(note_off) != MOL_OK ||
      runtime.submit(record_stop) != MOL_OK) {
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  molseq::SequenceDocument recording;
  if (runtime.copy_recording(recording) != MOL_OK || recording.events.empty()) {
    std::fprintf(stderr, "Runtime recording copy failed\n");
    return 1;
  }
  if (runtime.load_sequence(recording) != MOL_OK) return 1;

  molcontrol::BenchmarkResult benchmark;
  if (runtime.benchmark(48000u, benchmark) != MOL_OK || benchmark.frames != 48000u ||
      benchmark.non_finite_samples != 0u || benchmark.peak <= 0.0 ||
      !std::isfinite(benchmark.realtime_ratio) || benchmark.realtime_ratio <= 1.0) {
    std::fprintf(stderr, "Runtime benchmark failed\n");
    return 1;
  }
  std::string detail;
  if (!runtime.runtime_self_test(detail)) return 1;
  const molcontrol::RuntimeMetrics metrics = runtime.metrics();
  if (metrics.callbacks == 0u || metrics.rendered_frames == 0u || metrics.render_failures != 0u ||
      metrics.non_finite_samples != 0u || metrics.dropped_commands != 0u) {
    std::fprintf(stderr, "Runtime metrics are unhealthy\n");
    return 1;
  }
  const molcontrol::AudioStatus audio = runtime.audio_status();
  if (!audio.available || !audio.null_sink || audio.backend != "Null") return 1;
  mol_command_t preset = command(MOL_COMMAND_SET_PRESET);
  preset.payload.preset.preset = MOL_PRESET_VIOLIN;
  if (runtime.submit(preset) != MOL_OK) return 1;
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  state.struct_size = static_cast<std::uint32_t>(sizeof(state));
  if (runtime.snapshot(state) != MOL_OK || state.preset != MOL_PRESET_VIOLIN ||
      runtime.select_output("default") != MOL_OK)
    return 1;
  state.struct_size = static_cast<std::uint32_t>(sizeof(state));
  if (runtime.snapshot(state) != MOL_OK || state.preset != MOL_PRESET_VIOLIN) {
    std::fprintf(stderr, "Output restart did not preserve engine state\n");
    return 1;
  }
  const std::vector<molcontrol::DeviceInfo> inputs = runtime.input_devices();
  for (const molcontrol::DeviceInfo& input : inputs) {
    if (!input.is_physical_input) continue;
    if (runtime.attach_input(input.id) != MOL_OK || runtime.active_input_id() != input.id ||
        runtime.detach_input() != MOL_OK)
      return 1;
    break;
  }
  runtime.request_shutdown();
  if (!runtime.shutdown_requested()) return 1;
  runtime.stop();
  return 0;
}
