/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mol/sequence.h"

typedef struct fuzz_reader {
  const uint8_t* data;
  size_t size;
  size_t position;
  size_t max_read;
} fuzz_reader_t;

typedef struct fuzz_writer {
  uint8_t* data;
  size_t size;
  size_t capacity;
} fuzz_writer_t;

typedef struct fuzz_capture {
  mol_sequence_event_t* events;
  size_t count;
  size_t capacity;
  mol_frame_index_t previous_frame;
} fuzz_capture_t;

static size_t fuzz_read(void* user_data, uint8_t* output, size_t capacity) {
  fuzz_reader_t* reader = (fuzz_reader_t*)user_data;
  size_t remaining = reader->size - reader->position;
  size_t count = remaining < capacity ? remaining : capacity;
  if (count > reader->max_read) count = reader->max_read;
  memcpy(output, reader->data + reader->position, count);
  reader->position += count;
  return count;
}

static mol_result_t fuzz_write(void* user_data, const uint8_t* data, size_t size) {
  fuzz_writer_t* writer = (fuzz_writer_t*)user_data;
  if (size > writer->capacity - writer->size) return MOL_ERROR_IO;
  memcpy(writer->data + writer->size, data, size);
  writer->size += size;
  return MOL_OK;
}

static mol_result_t fuzz_capture_event(void* user_data, const mol_sequence_event_t* event) {
  fuzz_capture_t* capture = (fuzz_capture_t*)user_data;
  if (capture->count >= capture->capacity || mol_sequence_validate_event(event) != MOL_OK ||
      (capture->count != 0u && event->frame < capture->previous_frame)) {
    __builtin_trap();
  }
  capture->events[capture->count++] = *event;
  capture->previous_frame = event->frame;
  return MOL_OK;
}

static mol_result_t fuzz_capture_metadata(void* user_data, uint32_t chunk_type, const uint8_t* data,
                                          size_t size) {
  (void)user_data;
  (void)chunk_type;
  if ((data == NULL && size != 0u) || size > MOL_SEQUENCE_MAX_METADATA_SIZE) __builtin_trap();
  return MOL_OK;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  fuzz_reader_t reader = {data, size, 0u, size == 0u ? 1u : (size_t)(data[0] % 31u) + 1u};
  mol_sequence_config_t config = {0};
  mol_sequence_callbacks_t callbacks = {0};
  mol_sequence_writer_t sequence_writer = {0};
  fuzz_capture_t capture = {0};
  fuzz_writer_t writer = {0};
  mol_result_t result;

  if (size > 65536u) return 0;
  capture.capacity = size + 1u;
  capture.events = (mol_sequence_event_t*)malloc(capture.capacity * sizeof(*capture.events));
  writer.capacity = size + 1024u;
  writer.data = (uint8_t*)malloc(writer.capacity);
  if (capture.events == NULL || writer.data == NULL) {
    free(capture.events);
    free(writer.data);
    return 0;
  }

  config.struct_size = (uint32_t)sizeof(config);
  config.api_version = MOL_API_VERSION;
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = fuzz_capture_event;
  callbacks.on_metadata = fuzz_capture_metadata;
  callbacks.user_data = &capture;
  result = mol_sequence_read_stream(fuzz_read, &reader, &config, &callbacks);
  if (result == MOL_OK) {
    sequence_writer.struct_size = (uint32_t)sizeof(sequence_writer);
    sequence_writer.api_version = MOL_API_VERSION;
    if (mol_sequence_validate_config(&config) != MOL_OK ||
        mol_sequence_writer_init(&sequence_writer, &config, fuzz_write, &writer) != MOL_OK) {
      __builtin_trap();
    }
    for (size_t index = 0u; index < capture.count; ++index) {
      if (mol_sequence_writer_append(&sequence_writer, &capture.events[index]) != MOL_OK)
        __builtin_trap();
    }
    if (mol_sequence_writer_finalize(&sequence_writer) != MOL_OK) __builtin_trap();
    reader.data = writer.data;
    reader.size = writer.size;
    reader.position = 0u;
    reader.max_read = 7u;
    config.struct_size = (uint32_t)sizeof(config);
    config.api_version = MOL_API_VERSION;
    if (mol_sequence_read_stream(fuzz_read, &reader, &config, NULL) != MOL_OK) __builtin_trap();
  }

  free(capture.events);
  free(writer.data);
  return 0;
}
