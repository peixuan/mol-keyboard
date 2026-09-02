/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <string.h>

#include "sequence_store.h"

static int failures;
static const char* const kName = "mol_storage_test";
static const char* const kTarget = "./mol_storage_test.molseq";
static const char* const kTemporary = "./.mol_storage_test.tmp";
static const char* const kBackup = "./.mol_storage_test.bak";

#define EXPECT_TRUE(condition)                                                           \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      fprintf(stderr, "%s:%d expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static void cleanup(void) {
  (void)remove(kTarget);
  (void)remove(kTemporary);
  (void)remove(kBackup);
}

static int path_exists(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return 0;
  }
  (void)fclose(file);
  return 1;
}

static mol_sequence_event_t note_event(mol_command_type_t type, uint64_t frame,
                                       mol_gesture_id_t gesture) {
  mol_sequence_event_t event;
  memset(&event, 0, sizeof(event));
  event.struct_size = (uint32_t)sizeof(event);
  event.api_version = MOL_API_VERSION;
  event.frame = frame;
  event.command_type = type;
  event.source_id = 9u;
  event.gesture_id = gesture;
  event.payload.note.note = 60u;
  event.payload.note.velocity = type == MOL_COMMAND_NOTE_ON ? 0.8f : 0.0f;
  return event;
}

static void test_name_validation(void) {
  char long_name[MOL_SEQUENCE_STORE_MAX_NAME + 2u];
  memset(long_name, 'a', sizeof(long_name));
  long_name[sizeof(long_name) - 1u] = '\0';
  EXPECT_TRUE(mol_sequence_store_name_is_valid("lesson-01_A"));
  EXPECT_TRUE(!mol_sequence_store_name_is_valid(""));
  EXPECT_TRUE(!mol_sequence_store_name_is_valid("../lesson"));
  EXPECT_TRUE(!mol_sequence_store_name_is_valid("lesson.molseq"));
  EXPECT_TRUE(!mol_sequence_store_name_is_valid(long_name));
}

static void test_round_trip_and_overwrite(void) {
  mol_sequence_config_t config = mol_sequence_config_default(32000u);
  mol_sequence_config_t decoded;
  mol_sequence_event_t events[2];
  mol_sequence_event_t loaded[2];
  uint32_t count = 0u;
  cleanup();
  config.initial_state.preset = MOL_PRESET_FLUTE;
  events[0] = note_event(MOL_COMMAND_NOTE_ON, 10u, 1u);
  events[1] = note_event(MOL_COMMAND_NOTE_OFF, 3210u, 1u);
  EXPECT_TRUE(mol_sequence_store_save(".", kName, &config, events, 2u) == MOL_OK);
  memset(&decoded, 0, sizeof(decoded));
  memset(loaded, 0, sizeof(loaded));
  EXPECT_TRUE(mol_sequence_store_load(".", kName, &decoded, loaded, 2u, &count) == MOL_OK);
  EXPECT_TRUE(count == 2u && decoded.sample_rate == 32000u);
  EXPECT_TRUE(decoded.initial_state.preset == MOL_PRESET_FLUTE);
  EXPECT_TRUE(memcmp(events, loaded, sizeof(events)) == 0);
  EXPECT_TRUE(!path_exists(kTemporary));
  EXPECT_TRUE(!path_exists(kBackup));

  config.initial_state.preset = MOL_PRESET_CELLO;
  events[1].frame = 6410u;
  EXPECT_TRUE(mol_sequence_store_save(".", kName, &config, events, 2u) == MOL_OK);
  EXPECT_TRUE(mol_sequence_store_load(".", kName, &decoded, loaded, 2u, &count) == MOL_OK);
  EXPECT_TRUE(decoded.initial_state.preset == MOL_PRESET_CELLO);
  EXPECT_TRUE(loaded[1].frame == 6410u);
  cleanup();
}

static void test_capacity_is_transactional(void) {
  mol_sequence_config_t config = mol_sequence_config_default(32000u);
  mol_sequence_config_t decoded;
  mol_sequence_event_t events[2];
  mol_sequence_event_t untouched;
  mol_sequence_event_t expected;
  uint32_t count = 0u;
  cleanup();
  events[0] = note_event(MOL_COMMAND_NOTE_ON, 0u, 2u);
  events[1] = note_event(MOL_COMMAND_NOTE_OFF, 100u, 2u);
  EXPECT_TRUE(mol_sequence_store_save(".", kName, &config, events, 2u) == MOL_OK);
  memset(&decoded, 0xa5, sizeof(decoded));
  memset(&untouched, 0xa5, sizeof(untouched));
  memset(&expected, 0xa5, sizeof(expected));
  EXPECT_TRUE(mol_sequence_store_load(".", kName, &decoded, &untouched, 1u, &count) ==
              MOL_ERROR_BUFFER_TOO_SMALL);
  EXPECT_TRUE(count == 2u);
  EXPECT_TRUE(memcmp(&untouched, &expected, sizeof(expected)) == 0);
  cleanup();
}

static void test_corrupt_file_is_transactional(void) {
  mol_sequence_config_t config = mol_sequence_config_default(32000u);
  mol_sequence_config_t decoded;
  mol_sequence_config_t expected_config;
  mol_sequence_event_t event = note_event(MOL_COMMAND_NOTE_ON, 0u, 3u);
  mol_sequence_event_t loaded;
  mol_sequence_event_t expected_event;
  uint32_t count = 77u;
  FILE* file;
  int byte;
  cleanup();
  EXPECT_TRUE(mol_sequence_store_save(".", kName, &config, &event, 1u) == MOL_OK);
  file = fopen(kTarget, "rb+");
  EXPECT_TRUE(file != NULL);
  if (file != NULL) {
    EXPECT_TRUE(fseek(file, 40L, SEEK_SET) == 0);
    byte = fgetc(file);
    EXPECT_TRUE(byte != EOF);
    EXPECT_TRUE(fseek(file, 40L, SEEK_SET) == 0);
    EXPECT_TRUE(fputc(byte ^ 0x40, file) != EOF);
    EXPECT_TRUE(fclose(file) == 0);
  }
  memset(&decoded, 0xa5, sizeof(decoded));
  memset(&expected_config, 0xa5, sizeof(expected_config));
  memset(&loaded, 0xa5, sizeof(loaded));
  memset(&expected_event, 0xa5, sizeof(expected_event));
  EXPECT_TRUE(mol_sequence_store_load(".", kName, &decoded, &loaded, 1u, &count) ==
              MOL_ERROR_CORRUPT_DATA);
  EXPECT_TRUE(count == 77u);
  EXPECT_TRUE(memcmp(&decoded, &expected_config, sizeof(decoded)) == 0);
  EXPECT_TRUE(memcmp(&loaded, &expected_event, sizeof(loaded)) == 0);
  cleanup();
}

static void test_backup_recovery(void) {
  mol_sequence_config_t config = mol_sequence_config_default(32000u);
  mol_sequence_config_t decoded;
  mol_sequence_event_t event = note_event(MOL_COMMAND_NOTE_ON, 0u, 4u);
  mol_sequence_event_t loaded;
  uint32_t count = 0u;
  cleanup();
  EXPECT_TRUE(mol_sequence_store_save(".", kName, &config, &event, 1u) == MOL_OK);
  EXPECT_TRUE(rename(kTarget, kBackup) == 0);
  EXPECT_TRUE(mol_sequence_store_load(".", kName, &decoded, &loaded, 1u, &count) == MOL_OK);
  EXPECT_TRUE(count == 1u && loaded.gesture_id == 4u);
  EXPECT_TRUE(path_exists(kTarget));
  cleanup();
}

int main(void) {
  test_name_validation();
  test_round_trip_and_overwrite();
  test_capacity_is_transactional();
  test_corrupt_file_is_transactional();
  test_backup_recovery();
  cleanup();
  if (failures != 0) {
    fprintf(stderr, "%d ESP32 sequence-store test(s) failed\n", failures);
    return 1;
  }
  puts("ESP32 sequence-store tests passed");
  return 0;
}
