// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

#include "audio_runtime.hpp"

namespace {

mol_command_t command(mol_command_type_t type) {
  mol_command_t result{};
  result.struct_size = static_cast<std::uint32_t>(sizeof(result));
  result.api_version = MOL_API_VERSION;
  result.command_type = type;
  result.target_frame = MOL_FRAME_IMMEDIATE;
  result.source_id = UINT32_C(0x53545253);
  return result;
}

bool run_cycle(molkeyboardd::AudioRuntime& runtime, std::uint32_t cycle) {
  mol_command_t preset = command(MOL_COMMAND_SET_PRESET);
  mol_command_t record_start = command(MOL_COMMAND_RECORD_START);
  mol_command_t note_on = command(MOL_COMMAND_NOTE_ON);
  mol_command_t note_off = command(MOL_COMMAND_NOTE_OFF);
  mol_command_t record_stop = command(MOL_COMMAND_RECORD_STOP);
  preset.payload.preset.preset = cycle % MOL_PRESET_COUNT;
  note_on.gesture_id = static_cast<mol_gesture_id_t>(cycle) + 1u;
  note_on.payload.note.note = static_cast<std::uint8_t>(48u + cycle % 24u);
  note_on.payload.note.velocity = 0.7f;
  note_off.gesture_id = note_on.gesture_id;
  note_off.payload.note.note = note_on.payload.note.note;
  if (runtime.submit(preset) != MOL_OK || runtime.submit(record_start) != MOL_OK ||
      runtime.submit(note_on) != MOL_OK)
    return false;
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (runtime.select_output("default") != MOL_OK || runtime.submit(note_off) != MOL_OK ||
      runtime.submit(record_stop) != MOL_OK)
    return false;
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  mol_engine_state_t state{};
  state.struct_size = static_cast<std::uint32_t>(sizeof(state));
  if (runtime.snapshot(state) != MOL_OK || state.preset != preset.payload.preset.preset ||
      state.recording != 0u || state.recording_event_count == 0u)
    return false;
  molseq::SequenceDocument recording;
  if (runtime.copy_recording(recording) != MOL_OK || recording.events.empty()) return false;
  return cycle % 5u != 0u || runtime.load_sequence(recording) == MOL_OK;
}

}  // namespace

int main() {
  molkeyboardd::AudioRuntime runtime;
  std::string warning;
  if (!runtime.start(true, "default", warning)) {
    std::fprintf(stderr, "Null audio runtime startup failed: %s\n", warning.c_str());
    return 1;
  }
  for (std::uint32_t cycle = 0u; cycle < 30u; ++cycle) {
    if (!run_cycle(runtime, cycle)) {
      std::fprintf(stderr, "Audio restart/recording cycle %u failed\n", cycle);
      return 1;
    }
  }
  const molcontrol::RuntimeMetrics metrics = runtime.metrics();
  const molcontrol::AudioStatus status = runtime.audio_status();
  runtime.stop();
  if (metrics.callbacks == 0u || metrics.rendered_frames == 0u || metrics.render_failures != 0u ||
      metrics.non_finite_samples != 0u || metrics.dropped_commands != 0u || !status.available ||
      !status.null_sink) {
    std::fprintf(stderr, "Audio runtime metrics were unhealthy after restart stress\n");
    return 1;
  }
  std::printf("restart_cycles=30 callbacks=%llu rendered_frames=%llu render_failures=%llu\n",
              static_cast<unsigned long long>(metrics.callbacks),
              static_cast<unsigned long long>(metrics.rendered_frames),
              static_cast<unsigned long long>(metrics.render_failures));
  return 0;
}
