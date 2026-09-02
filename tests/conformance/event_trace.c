/* SPDX-License-Identifier: Apache-2.0 */
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union trace_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[262144];
} trace_storage_t;

static trace_storage_t storage;

static mol_command_t command_at(mol_command_type_t type, mol_frame_index_t frame) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  command.source_id = 42u;
  return command;
}

static mol_command_t note_at(mol_command_type_t type, mol_frame_index_t frame,
                             mol_gesture_id_t gesture, uint8_t note) {
  mol_command_t command = command_at(type, frame);
  command.gesture_id = gesture;
  command.payload.note.note = note;
  command.payload.note.velocity = 0.8f;
  return command;
}

static int submit(mol_engine_t* engine, const mol_command_t* command) {
  mol_result_t result = mol_engine_submit(engine, command);
  if (result != MOL_OK) {
    (void)fprintf(stderr, "submit failed for command %" PRIu32 ": %s\n", command->command_type,
                  mol_result_string(result));
    return 0;
  }
  return 1;
}

static uint64_t hash_byte(uint64_t hash, uint8_t value) {
  return (hash ^ value) * UINT64_C(1099511628211);
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
  uint32_t shift;
  for (shift = 0u; shift < 64u; shift += 8u) {
    hash = hash_byte(hash, (uint8_t)(value >> shift));
  }
  return hash;
}

static uint64_t hash_event(uint64_t hash, const mol_event_t* event) {
  uint32_t index;
  hash = hash_u64(hash, event->event_type);
  hash = hash_u64(hash, event->source_id);
  hash = hash_u64(hash, event->frame);
  hash = hash_u64(hash, event->gesture_id);
  for (index = 0u; index < sizeof(event->payload); ++index) {
    hash = hash_byte(hash, event->payload[index]);
  }
  return hash;
}

int main(void) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t transpose = command_at(MOL_COMMAND_SET_TRANSPOSE, 0u);
  mol_command_t scale = command_at(MOL_COMMAND_SET_SCALE, 0u);
  mol_command_t chord = command_at(MOL_COMMAND_SET_CHORD_MODE, 0u);
  mol_command_t tempo = command_at(MOL_COMMAND_SET_TEMPO, 0u);
  mol_command_t signature = command_at(MOL_COMMAND_SET_TIME_SIGNATURE, 0u);
  mol_command_t metronome = command_at(MOL_COMMAND_SET_METRONOME, 0u);
  mol_command_t arpeggiator = command_at(MOL_COMMAND_SET_ARPEGGIATOR, 0u);
  mol_command_t start = command_at(MOL_COMMAND_TRANSPORT_START, 0u);
  mol_command_t first = note_at(MOL_COMMAND_NOTE_ON, 0u, 100u, 60u);
  mol_command_t second = note_at(MOL_COMMAND_NOTE_ON, 12000u, 101u, 64u);
  mol_command_t pedal_on = command_at(MOL_COMMAND_SUSTAIN, 18000u);
  mol_command_t first_off = note_at(MOL_COMMAND_NOTE_OFF, 20000u, 100u, 0u);
  mol_command_t chord_off = command_at(MOL_COMMAND_SET_CHORD_MODE, 24000u);
  mol_command_t arpeggiator_off = command_at(MOL_COMMAND_SET_ARPEGGIATOR, 24000u);
  mol_command_t portamento = command_at(MOL_COMMAND_SET_PORTAMENTO, 24000u);
  mol_command_t third = note_at(MOL_COMMAND_NOTE_ON, 24000u, 102u, 67u);
  mol_command_t second_off = note_at(MOL_COMMAND_NOTE_OFF, 28000u, 101u, 0u);
  mol_command_t pedal_off = command_at(MOL_COMMAND_SUSTAIN, 32000u);
  mol_command_t fourth = note_at(MOL_COMMAND_NOTE_ON, 36000u, 103u, 72u);
  mol_command_t fourth_off = note_at(MOL_COMMAND_NOTE_OFF, 42000u, 103u, 0u);
  mol_command_t third_off = note_at(MOL_COMMAND_NOTE_OFF, 48000u, 102u, 0u);
  mol_command_t stop = command_at(MOL_COMMAND_TRANSPORT_STOP, 50000u);
  float output[113];
  mol_event_t events[256];
  mol_engine_state_t state = {0};
  uint64_t hash = UINT64_C(14695981039346656037);
  uint32_t event_count;
  uint32_t rendered = 0u;

  config.channel_count = 1u;
  if (mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) != MOL_OK) {
    (void)fprintf(stderr, "engine initialization failed\n");
    return 1;
  }
  transpose.payload.integer.value = -2;
  scale.payload.scale.type = MOL_SCALE_DORIAN;
  scale.payload.scale.tonic = 2u;
  scale.payload.scale.mapping = MOL_SCALE_MAP_NEAREST;
  chord.payload.integer.value = (int32_t)MOL_CHORD_MINOR_7;
  tempo.payload.scalar.value = 120.0f;
  signature.payload.time_signature.numerator = 6u;
  signature.payload.time_signature.denominator = 8u;
  metronome.payload.metronome.enabled = 1u;
  metronome.payload.metronome.level = 0.25f;
  arpeggiator.payload.arpeggiator.mode = MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC;
  arpeggiator.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_EIGHTH_TRIPLET;
  arpeggiator.payload.arpeggiator.gate = 0.625f;
  arpeggiator.payload.arpeggiator.random_seed = UINT32_C(0x10203040);
  arpeggiator.payload.arpeggiator.octaves = 2u;
  pedal_on.payload.scalar.value = 1.0f;
  chord_off.payload.integer.value = (int32_t)MOL_CHORD_OFF;
  arpeggiator_off.payload.arpeggiator.mode = MOL_ARPEGGIATOR_OFF;
  arpeggiator_off.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
  arpeggiator_off.payload.arpeggiator.gate = 0.5f;
  arpeggiator_off.payload.arpeggiator.random_seed = UINT32_C(0x10203040);
  arpeggiator_off.payload.arpeggiator.octaves = 1u;
  portamento.payload.portamento.mode = MOL_PORTAMENTO_ALWAYS;
  portamento.payload.portamento.time_ms = 80.0f;
  pedal_off.payload.scalar.value = 0.0f;

  if (!submit(engine, &transpose) || !submit(engine, &scale) || !submit(engine, &chord) ||
      !submit(engine, &tempo) || !submit(engine, &signature) || !submit(engine, &metronome) ||
      !submit(engine, &arpeggiator) || !submit(engine, &start) || !submit(engine, &first) ||
      !submit(engine, &second) || !submit(engine, &pedal_on) || !submit(engine, &first_off) ||
      !submit(engine, &chord_off) || !submit(engine, &arpeggiator_off) ||
      !submit(engine, &portamento) || !submit(engine, &third) || !submit(engine, &second_off) ||
      !submit(engine, &pedal_off) || !submit(engine, &fourth) || !submit(engine, &fourth_off) ||
      !submit(engine, &third_off) || !submit(engine, &stop)) {
    mol_engine_shutdown(engine);
    return 1;
  }

  while (rendered < 100000u) {
    uint32_t block = 100000u - rendered;
    if (block > 113u) {
      block = 113u;
    }
    if (mol_engine_render_interleaved_f32(engine, output, block, 1u) != MOL_OK) {
      (void)fprintf(stderr, "render failed\n");
      mol_engine_shutdown(engine);
      return 1;
    }
    for (uint32_t index = 0u; index < block; ++index) {
      if (!isfinite(output[index])) {
        (void)fprintf(stderr, "non-finite output\n");
        mol_engine_shutdown(engine);
        return 1;
      }
    }
    rendered += block;
  }
  event_count = mol_engine_poll_events(engine, events, 256u);
  for (uint32_t index = 0u; index < event_count; ++index) {
    hash = hash_event(hash, &events[index]);
  }
  state.struct_size = (uint32_t)sizeof(state);
  if (mol_engine_get_state(engine, &state) != MOL_OK) {
    mol_engine_shutdown(engine);
    return 1;
  }
  (void)printf("events=%" PRIu32 " hash=%016" PRIx64 " transport=%" PRIu64 " voices=%" PRIu32
               " gestures=%" PRIu32 "\n",
               event_count, hash, state.transport_frame, state.active_voices,
               state.active_gestures);
  mol_engine_shutdown(engine);
  return 0;
}
