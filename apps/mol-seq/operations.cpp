// SPDX-License-Identifier: Apache-2.0
#include "operations.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace molseq {
namespace {

bool is_state_control(mol_command_type_t type) {
  switch (type) {
    case MOL_COMMAND_PITCH_BEND:
    case MOL_COMMAND_SUSTAIN:
    case MOL_COMMAND_SET_MASTER_GAIN:
    case MOL_COMMAND_SET_PRESET:
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
    case MOL_COMMAND_SET_TRANSPOSE:
    case MOL_COMMAND_SET_SCALE:
    case MOL_COMMAND_SET_CHORD_MODE:
    case MOL_COMMAND_SET_ARPEGGIATOR:
    case MOL_COMMAND_SET_TEMPO:
    case MOL_COMMAND_SET_TIME_SIGNATURE:
    case MOL_COMMAND_SET_METRONOME:
    case MOL_COMMAND_SET_PORTAMENTO:
      return true;
    default:
      return false;
  }
}

void append_checked(std::vector<mol_sequence_event_t>& output, mol_sequence_event_t event) {
  if (output.size() >= MOL_SEQUENCE_MAX_EVENTS)
    throw std::runtime_error("sequence operation exceeds event limit");
  output.push_back(event);
}

}  // namespace

SequenceDocument trim_sequence(const SequenceDocument& input, std::uint64_t start_frame,
                               std::uint64_t end_frame) {
  if (start_frame >= end_frame) throw std::runtime_error("trim range must be non-empty");
  SequenceDocument output;
  output.config = input.config;
  output.metadata = input.metadata;
  std::map<mol_command_type_t, mol_sequence_event_t> state_controls;
  std::map<mol_parameter_id_t, mol_sequence_event_t> parameters;
  std::map<mol_gesture_id_t, mol_sequence_event_t> active_notes;
  mol_sequence_event_t transport_state{};
  mol_sequence_event_t transport_seek{};
  bool have_transport_state = false;
  bool have_transport_seek = false;
  const std::uint64_t duration = end_frame - start_frame;

  for (const mol_sequence_event_t& source : input.events) {
    if (source.frame < start_frame) {
      if (is_state_control(source.command_type)) state_controls[source.command_type] = source;
      if (source.command_type == MOL_COMMAND_SET_PARAMETER)
        parameters[source.payload.parameter.parameter] = source;
      if (source.command_type == MOL_COMMAND_TRANSPORT_START ||
          source.command_type == MOL_COMMAND_TRANSPORT_STOP) {
        transport_state = source;
        have_transport_state = true;
      }
      if (source.command_type == MOL_COMMAND_TRANSPORT_SEEK) {
        transport_seek = source;
        have_transport_seek = true;
      }
      if (source.command_type == MOL_COMMAND_NOTE_ON) active_notes[source.gesture_id] = source;
      if (source.command_type == MOL_COMMAND_NOTE_OFF) active_notes.erase(source.gesture_id);
      continue;
    }
    break;
  }

  for (const auto& entry : state_controls) {
    mol_sequence_event_t event = entry.second;
    event.frame = 0u;
    append_checked(output.events, event);
  }
  for (const auto& entry : parameters) {
    mol_sequence_event_t event = entry.second;
    event.frame = 0u;
    append_checked(output.events, event);
  }
  if (have_transport_seek) {
    transport_seek.frame = 0u;
    append_checked(output.events, transport_seek);
  }
  if (have_transport_state) {
    transport_state.frame = 0u;
    append_checked(output.events, transport_state);
  }
  for (const auto& entry : active_notes) {
    mol_sequence_event_t event = entry.second;
    event.frame = 0u;
    append_checked(output.events, event);
  }

  std::map<mol_gesture_id_t, mol_sequence_event_t> selected_active = active_notes;
  for (const mol_sequence_event_t& source : input.events) {
    if (source.frame < start_frame) continue;
    if (source.frame >= end_frame) break;
    mol_sequence_event_t event = source;
    event.frame -= start_frame;
    append_checked(output.events, event);
    if (event.command_type == MOL_COMMAND_NOTE_ON) selected_active[event.gesture_id] = event;
    if (event.command_type == MOL_COMMAND_NOTE_OFF) selected_active.erase(event.gesture_id);
  }
  for (const auto& entry : selected_active) {
    mol_sequence_event_t event = entry.second;
    event.command_type = MOL_COMMAND_NOTE_OFF;
    event.frame = duration;
    event.payload.note.velocity = 0.0f;
    append_checked(output.events, event);
  }
  std::stable_sort(output.events.begin(), output.events.end(),
                   [](const mol_sequence_event_t& left, const mol_sequence_event_t& right) {
                     return left.frame < right.frame;
                   });
  return output;
}

SequenceDocument merge_sequences(const std::vector<SequenceDocument>& inputs) {
  if (inputs.size() < 2u) throw std::runtime_error("merge requires at least two sequences");
  SequenceDocument output;
  output.config = inputs.front().config;
  mol_gesture_id_t next_gesture = 1u;
  for (const SequenceDocument& input : inputs) {
    if (input.config.sample_rate != output.config.sample_rate ||
        input.config.time_base != output.config.time_base)
      throw std::runtime_error("merged sequences must share sample rate and time base");
    output.metadata.insert(output.metadata.end(), input.metadata.begin(), input.metadata.end());
    std::map<mol_gesture_id_t, mol_gesture_id_t> gesture_map;
    for (mol_sequence_event_t event : input.events) {
      if (event.gesture_id != 0u) {
        auto inserted = gesture_map.emplace(event.gesture_id, next_gesture);
        if (inserted.second) {
          if (next_gesture == UINT64_MAX) throw std::runtime_error("gesture identifier overflow");
          ++next_gesture;
        }
        event.gesture_id = inserted.first->second;
      }
      append_checked(output.events, event);
    }
  }
  std::stable_sort(output.events.begin(), output.events.end(),
                   [](const mol_sequence_event_t& left, const mol_sequence_event_t& right) {
                     return left.frame < right.frame;
                   });
  return output;
}

SequenceDocument quantize_sequence(const SequenceDocument& input, std::uint64_t grid_frames) {
  if (grid_frames == 0u) throw std::runtime_error("quantize grid must be nonzero");
  SequenceDocument output = input;
  for (mol_sequence_event_t& event : output.events) {
    const std::uint64_t quotient = event.frame / grid_frames;
    const std::uint64_t remainder = event.frame % grid_frames;
    std::uint64_t rounded = quotient;
    if (remainder >= grid_frames / 2u + grid_frames % 2u) {
      if (rounded == UINT64_MAX) throw std::runtime_error("quantized frame overflow");
      ++rounded;
    }
    if (rounded != 0u && grid_frames > (UINT64_MAX - 1u) / rounded)
      throw std::runtime_error("quantized frame overflow");
    event.frame = rounded * grid_frames;
  }
  std::stable_sort(output.events.begin(), output.events.end(),
                   [](const mol_sequence_event_t& left, const mol_sequence_event_t& right) {
                     return left.frame < right.frame;
                   });
  return output;
}

}  // namespace molseq
