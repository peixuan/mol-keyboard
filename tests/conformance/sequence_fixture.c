/* SPDX-License-Identifier: Apache-2.0 */
#include "sequence_fixture.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef MOL_SEQUENCE_FIXTURE_STANDALONE
#include <inttypes.h>
#include <stdio.h>
#endif

#include "mol/sequence.h"
#include "mol_sequence_fixture_bytes.h"

typedef struct mol_fixture_reader {
  size_t position;
} mol_fixture_reader_t;

typedef struct mol_fixture_capture {
  mol_sequence_fixture_summary_t* summary;
  int valid;
} mol_fixture_capture_t;

static size_t mol_fixture_read(void* user_data, uint8_t* output, size_t capacity) {
  mol_fixture_reader_t* reader = (mol_fixture_reader_t*)user_data;
  size_t remaining = kMolSequenceFixtureSize - reader->position;
  size_t count = remaining < capacity ? remaining : capacity;
  if (count > 11u) count = 11u;
  memcpy(output, kMolSequenceFixtureBytes + reader->position, count);
  reader->position += count;
  return count;
}

static mol_result_t mol_fixture_event(void* user_data, const mol_sequence_event_t* event) {
  mol_fixture_capture_t* capture = (mol_fixture_capture_t*)user_data;
  mol_sequence_fixture_summary_t* summary = capture->summary;
  if (summary->event_count != 0u && event->frame < summary->final_frame) capture->valid = 0;
  ++summary->event_count;
  summary->final_frame = event->frame;
  switch (event->command_type) {
    case MOL_COMMAND_NOTE_ON:
      ++summary->note_on_count;
      break;
    case MOL_COMMAND_NOTE_OFF:
      ++summary->note_off_count;
      break;
    case MOL_COMMAND_TRANSPORT_START:
      ++summary->transport_start_count;
      break;
    case MOL_COMMAND_TRANSPORT_STOP:
      ++summary->transport_stop_count;
      break;
    default:
      break;
  }
  return MOL_OK;
}

static mol_result_t mol_fixture_metadata(void* user_data, uint32_t chunk_type, const uint8_t* data,
                                         size_t size) {
  static const uint8_t expected_name[] = "Scale Study";
  mol_fixture_capture_t* capture = (mol_fixture_capture_t*)user_data;
  ++capture->summary->metadata_count;
  if (chunk_type != UINT32_C(0x454d414e) || size != sizeof(expected_name) - 1u ||
      memcmp(data, expected_name, sizeof(expected_name) - 1u) != 0) {
    capture->valid = 0;
  }
  return MOL_OK;
}

int mol_sequence_fixture_verify(mol_sequence_fixture_summary_t* summary) {
  mol_fixture_reader_t reader = {0};
  mol_fixture_capture_t capture;
  mol_sequence_config_t config = {0};
  mol_sequence_callbacks_t callbacks = {0};
  if (summary == NULL) return 0;
  memset(summary, 0, sizeof(*summary));
  capture.summary = summary;
  capture.valid = 1;
  config.struct_size = (uint32_t)sizeof(config);
  config.api_version = MOL_API_VERSION;
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = mol_fixture_event;
  callbacks.on_metadata = mol_fixture_metadata;
  callbacks.user_data = &capture;
  if (mol_sequence_read_stream(mol_fixture_read, &reader, &config, &callbacks) != MOL_OK) return 0;
  summary->sample_rate = config.sample_rate;
  summary->time_base = config.time_base;
  return capture.valid && config.sample_rate == 48000u && config.time_base == 48000u &&
         config.initial_state.preset == MOL_PRESET_GRAND_PIANO &&
         config.initial_state.master_gain == 0.25f && config.initial_state.tempo == 120.0f &&
         summary->event_count == 12u && summary->metadata_count == 1u &&
         summary->note_on_count == 4u && summary->note_off_count == 4u &&
         summary->transport_start_count == 1u && summary->transport_stop_count == 1u &&
         summary->final_frame == UINT64_C(108000);
}

#ifdef MOL_SEQUENCE_FIXTURE_STANDALONE
int main(void) {
  mol_sequence_fixture_summary_t summary;
  if (!mol_sequence_fixture_verify(&summary)) return 1;
  (void)printf("sample_rate=%" PRIu32 " time_base=%" PRIu32 " events=%" PRIu32 " metadata=%" PRIu32
               " note_on=%" PRIu32 " note_off=%" PRIu32 " final=%" PRIu64 "\n",
               summary.sample_rate, summary.time_base, summary.event_count, summary.metadata_count,
               summary.note_on_count, summary.note_off_count, summary.final_frame);
  return 0;
}
#endif
