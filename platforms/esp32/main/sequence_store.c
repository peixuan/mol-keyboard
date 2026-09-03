/* SPDX-License-Identifier: Apache-2.0 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "sequence_store.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#define MOL_SEQUENCE_STORE_PATH_SIZE 96u

typedef struct sequence_file_reader {
  FILE* file;
} sequence_file_reader_t;

typedef struct sequence_capture {
  mol_sequence_event_t* events;
  uint32_t capacity;
  uint32_t count;
} sequence_capture_t;

static mol_sequence_event_t staging_events[MOL_SEQUENCE_STORE_MAX_EVENTS];

static int is_name_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_';
}

int mol_sequence_store_name_is_valid(const char* name) {
  size_t length = 0u;
  if (name == NULL) {
    return 0;
  }
  while (name[length] != '\0') {
    if (length >= MOL_SEQUENCE_STORE_MAX_NAME || !is_name_character(name[length])) {
      return 0;
    }
    ++length;
  }
  return length != 0u;
}

static mol_result_t make_path(const char* base_path, const char* name, const char* prefix,
                              const char* suffix, char output[MOL_SEQUENCE_STORE_PATH_SIZE]) {
  int written;
  if (base_path == NULL || base_path[0] == '\0' || !mol_sequence_store_name_is_valid(name)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  written =
      snprintf(output, MOL_SEQUENCE_STORE_PATH_SIZE, "%s/%s%s%s", base_path, prefix, name, suffix);
  if (written < 0 || (size_t)written >= MOL_SEQUENCE_STORE_PATH_SIZE) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return MOL_OK;
}

static mol_result_t write_file(void* user_data, const uint8_t* data, size_t size) {
  FILE* file = (FILE*)user_data;
  return fwrite(data, 1u, size, file) == size ? MOL_OK : MOL_ERROR_IO;
}

static size_t read_file(void* user_data, uint8_t* data, size_t capacity) {
  sequence_file_reader_t* reader = (sequence_file_reader_t*)user_data;
  return fread(data, 1u, capacity, reader->file);
}

static mol_result_t capture_event(void* user_data, const mol_sequence_event_t* event) {
  sequence_capture_t* capture = (sequence_capture_t*)user_data;
  if (capture->count >= capture->capacity) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  capture->events[capture->count++] = *event;
  return MOL_OK;
}

static mol_result_t sync_file(FILE* file) {
  if (fflush(file) != 0 || ferror(file)) {
    return MOL_ERROR_IO;
  }
#if defined(_WIN32)
  return _commit(_fileno(file)) == 0 ? MOL_OK : MOL_ERROR_IO;
#else
  return fsync(fileno(file)) == 0 ? MOL_OK : MOL_ERROR_IO;
#endif
}

static int file_exists(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return 0;
  }
  (void)fclose(file);
  return 1;
}

static mol_result_t parse_file(const char* path, mol_sequence_config_t* config,
                               mol_sequence_event_t* events, uint32_t capacity,
                               uint32_t* event_count, int* exists) {
  mol_sequence_callbacks_t callbacks;
  sequence_capture_t capture;
  sequence_file_reader_t reader;
  mol_sequence_config_t decoded;
  mol_result_t result;
  FILE* file = fopen(path, "rb");
  *exists = file != NULL;
  if (file == NULL) {
    return MOL_ERROR_IO;
  }
  memset(&decoded, 0, sizeof(decoded));
  decoded.struct_size = (uint32_t)sizeof(decoded);
  decoded.api_version = MOL_API_VERSION;
  reader.file = file;
  result = mol_sequence_read_stream(read_file, &reader, &decoded, NULL);
  if (ferror(file)) {
    result = MOL_ERROR_IO;
  }
  if (fclose(file) != 0 && result == MOL_OK) {
    result = MOL_ERROR_IO;
  }
  if (result != MOL_OK) {
    return result;
  }

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = capture_event;
  callbacks.user_data = &capture;
  capture.events = staging_events;
  capture.capacity = MOL_SEQUENCE_STORE_MAX_EVENTS;
  capture.count = 0u;
  file = fopen(path, "rb");
  if (file == NULL) {
    return MOL_ERROR_IO;
  }
  reader.file = file;
  result = mol_sequence_read_stream(read_file, &reader, &decoded, &callbacks);
  if (ferror(file)) {
    result = MOL_ERROR_IO;
  }
  if (fclose(file) != 0 && result == MOL_OK) {
    result = MOL_ERROR_IO;
  }
  if (result != MOL_OK) {
    return result;
  }
  *event_count = capture.count;
  if (capture.count > capacity) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  if (capture.count != 0u && events == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *config = decoded;
  if (capture.count != 0u) {
    memcpy(events, staging_events, sizeof(events[0]) * capture.count);
  }
  return MOL_OK;
}

mol_result_t mol_sequence_store_save(const char* base_path, const char* name,
                                     const mol_sequence_config_t* config,
                                     const mol_sequence_event_t* events, uint32_t event_count) {
  char target[MOL_SEQUENCE_STORE_PATH_SIZE];
  char temporary[MOL_SEQUENCE_STORE_PATH_SIZE];
  char backup[MOL_SEQUENCE_STORE_PATH_SIZE];
  mol_sequence_writer_t writer;
  mol_result_t result;
  uint32_t index;
  int target_existed;
  FILE* file;
  if (event_count > MOL_SEQUENCE_STORE_MAX_EVENTS || (event_count != 0u && events == NULL) ||
      make_path(base_path, name, "", ".molseq", target) != MOL_OK ||
      make_path(base_path, name, ".", ".tmp", temporary) != MOL_OK ||
      make_path(base_path, name, ".", ".bak", backup) != MOL_OK) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  (void)remove(temporary);
  file = fopen(temporary, "wb");
  if (file == NULL) {
    return MOL_ERROR_IO;
  }
  memset(&writer, 0, sizeof(writer));
  writer.struct_size = (uint32_t)sizeof(writer);
  writer.api_version = MOL_API_VERSION;
  result = mol_sequence_writer_init(&writer, config, write_file, file);
  for (index = 0u; index < event_count && result == MOL_OK; ++index) {
    result = mol_sequence_writer_append(&writer, &events[index]);
  }
  if (result == MOL_OK) {
    result = mol_sequence_writer_finalize(&writer);
  }
  if (result == MOL_OK) {
    result = sync_file(file);
  }
  if (fclose(file) != 0 && result == MOL_OK) {
    result = MOL_ERROR_IO;
  }
  if (result != MOL_OK) {
    (void)remove(temporary);
    return result;
  }

  if (remove(backup) != 0 && errno != ENOENT) {
    (void)remove(temporary);
    return MOL_ERROR_IO;
  }
  target_existed = file_exists(target);
  if (target_existed && rename(target, backup) != 0) {
    (void)remove(temporary);
    return MOL_ERROR_IO;
  }
  if (rename(temporary, target) != 0) {
    if (target_existed) {
      (void)rename(backup, target);
    }
    (void)remove(temporary);
    return MOL_ERROR_IO;
  }
  if (target_existed) {
    (void)remove(backup);
  }
  return MOL_OK;
}

mol_result_t mol_sequence_store_load(const char* base_path, const char* name,
                                     mol_sequence_config_t* config, mol_sequence_event_t* events,
                                     uint32_t capacity, uint32_t* event_count) {
  char target[MOL_SEQUENCE_STORE_PATH_SIZE];
  char backup[MOL_SEQUENCE_STORE_PATH_SIZE];
  mol_result_t target_result;
  mol_result_t backup_result;
  int target_exists = 0;
  int backup_exists = 0;
  if (config == NULL || event_count == NULL ||
      make_path(base_path, name, "", ".molseq", target) != MOL_OK ||
      make_path(base_path, name, ".", ".bak", backup) != MOL_OK) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  target_result = parse_file(target, config, events, capacity, event_count, &target_exists);
  if (target_result == MOL_OK) {
    (void)remove(backup);
    return MOL_OK;
  }
  backup_result = parse_file(backup, config, events, capacity, event_count, &backup_exists);
  if (backup_result == MOL_OK) {
    if (target_exists) {
      (void)remove(target);
    }
    (void)rename(backup, target);
    return MOL_OK;
  }
  if (target_exists) {
    return target_result;
  }
  return backup_exists ? backup_result : MOL_ERROR_IO;
}
