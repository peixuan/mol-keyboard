/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_SEQUENCE_H_
#define MOL_SEQUENCE_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/command.h"
#include "mol/export.h"
#include "mol/music.h"
#include "mol/patch.h"
#include "mol/result.h"
#include "mol/transport.h"
#include "mol/version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_SEQUENCE_FORMAT_VERSION 1u
#define MOL_SEQUENCE_HEADER_SIZE 112u
#define MOL_SEQUENCE_MAX_EVENTS UINT32_C(1000000)
#define MOL_SEQUENCE_MAX_METADATA_SIZE 256u
#define MOL_SEQUENCE_MAX_RECORD_SIZE 512u

typedef struct mol_sequence_initial_state {
  uint32_t struct_size;
  uint32_t api_version;
  mol_preset_id_t preset;
  float master_gain;
  float tempo;
  uint8_t time_signature_numerator;
  uint8_t time_signature_denominator;
  int8_t octave_shift;
  int8_t transpose;
  mol_scale_type_t scale_type;
  uint8_t scale_tonic;
  uint8_t scale_mapping;
  uint8_t reserved_0[2];
  mol_chord_mode_t chord_mode;
  mol_arpeggiator_mode_t arpeggiator_mode;
  mol_arpeggiator_rate_t arpeggiator_rate;
  float arpeggiator_gate;
  uint32_t arpeggiator_random_seed;
  uint8_t arpeggiator_octaves;
  uint8_t reserved_1[3];
  float sustain;
  float pitch_bend;
  mol_portamento_mode_t portamento_mode;
  float portamento_time_ms;
  float metronome_level;
  uint8_t metronome_enabled;
  uint8_t reserved_2[3];
} mol_sequence_initial_state_t;

typedef struct mol_sequence_config {
  uint32_t struct_size;
  uint32_t api_version;
  uint32_t sample_rate;
  uint32_t time_base;
  mol_sequence_initial_state_t initial_state;
} mol_sequence_config_t;

typedef struct mol_sequence_event {
  uint32_t struct_size;
  uint32_t api_version;
  mol_frame_index_t frame;
  mol_command_type_t command_type;
  uint32_t source_id;
  mol_gesture_id_t gesture_id;
  mol_command_payload_t payload;
} mol_sequence_event_t;

typedef mol_result_t (*mol_sequence_write_fn)(void* user_data, const uint8_t* data, size_t size);
typedef size_t (*mol_sequence_read_fn)(void* user_data, uint8_t* data, size_t capacity);
typedef mol_result_t (*mol_sequence_event_fn)(void* user_data, const mol_sequence_event_t* event);
typedef mol_result_t (*mol_sequence_metadata_fn)(void* user_data, uint32_t chunk_type,
                                                 const uint8_t* data, size_t size);

typedef struct mol_sequence_callbacks {
  uint32_t struct_size;
  uint32_t api_version;
  mol_sequence_event_fn on_event;
  mol_sequence_metadata_fn on_metadata;
  void* user_data;
} mol_sequence_callbacks_t;

typedef struct mol_sequence_writer {
  uint32_t struct_size;
  uint32_t api_version;
  mol_sequence_write_fn write;
  void* user_data;
  mol_frame_index_t previous_frame;
  uint32_t event_count;
  uint32_t crc_state;
  uint8_t active;
  uint8_t finalized;
  uint8_t reserved[2];
} mol_sequence_writer_t;

/** Returns a deterministic v1 initial state. */
MOL_API mol_sequence_initial_state_t mol_sequence_initial_state_default(void);

/** Returns a deterministic v1 sequence configuration for a sample rate. */
MOL_API mol_sequence_config_t mol_sequence_config_default(uint32_t sample_rate);

/** Validates versioned configuration fields and fixed v1 bounds. */
MOL_API mol_result_t mol_sequence_validate_config(const mol_sequence_config_t* config);

/** Validates one canonical event without writing it. */
MOL_API mol_result_t mol_sequence_validate_event(const mol_sequence_event_t* event);

/** Starts a forward-only stream. A valid file exists only after finalize succeeds. */
MOL_API mol_result_t mol_sequence_writer_init(mol_sequence_writer_t* writer,
                                              const mol_sequence_config_t* config,
                                              mol_sequence_write_fn write, void* user_data);

/** Appends one canonical event. Frames must be monotonic and finite. */
MOL_API mol_result_t mol_sequence_writer_append(mol_sequence_writer_t* writer,
                                                const mol_sequence_event_t* event);

/** Appends a bounded optional metadata chunk identified by a little-endian FourCC. */
MOL_API mol_result_t mol_sequence_writer_add_metadata(mol_sequence_writer_t* writer,
                                                      uint32_t chunk_type, const uint8_t* data,
                                                      size_t size);

/** Writes the event count, final frame, and CRC32 completion record. */
MOL_API mol_result_t mol_sequence_writer_finalize(mol_sequence_writer_t* writer);

/**
 * Parses a forward-only stream with bounded scratch memory.
 *
 * Callbacks may observe records before the final CRC is known. Validate untrusted
 * input in a separate pass when transactional delivery is required.
 */
MOL_API mol_result_t mol_sequence_read_stream(mol_sequence_read_fn read, void* read_user_data,
                                              mol_sequence_config_t* out_config,
                                              const mol_sequence_callbacks_t* callbacks);

#ifdef __cplusplus
}
#endif

#endif /* MOL_SEQUENCE_H_ */
