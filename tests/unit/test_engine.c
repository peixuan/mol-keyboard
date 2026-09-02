/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mol/mol.h"

typedef union test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} test_storage_t;

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static void test_lifecycle(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_engine_state_t state = {0};
  float output[64] = {1.0f};
  size_t required = mol_engine_query_memory(&config);

  EXPECT_TRUE(required > 0u);
  EXPECT_TRUE(required <= sizeof(storage.bytes));
  EXPECT_TRUE(mol_engine_memory_alignment() <= _Alignof(test_storage_t));
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(engine != NULL);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & MOL_CAPABILITY_CALLER_MEMORY) != 0u);

  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 32u, 2u) == MOL_OK);
  for (size_t index = 0u; index < 64u; ++index) {
    EXPECT_TRUE(output[index] == 0.0f);
  }

  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.current_frame == 32u);
  EXPECT_TRUE(state.sample_rate == 48000u);
  EXPECT_TRUE(state.active_voices == 0u);

  mol_engine_reset(engine);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.current_frame == 0u);
  mol_engine_shutdown(engine);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_ERROR_INVALID_ARGUMENT);
}

static void test_validation(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = (mol_engine_t*)(uintptr_t)1u;

  EXPECT_TRUE(mol_get_api_version() == MOL_API_VERSION);
  EXPECT_TRUE(mol_get_version_string() != NULL);
  EXPECT_TRUE(mol_engine_init(storage.bytes, 1u, &config, &engine) ==
              MOL_ERROR_INSUFFICIENT_MEMORY);
  EXPECT_TRUE(engine == NULL);

  config.api_version = 0u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) ==
              MOL_ERROR_UNSUPPORTED_VERSION);
  EXPECT_TRUE(engine == NULL);
  config = mol_engine_config_default();
  config.sample_rate = 12345u;
  EXPECT_TRUE(mol_engine_query_memory(&config) > 0u);
  config.sample_rate = 7999u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config.sample_rate = 192001u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
}

static void test_tiny_embedded_budget(void) {
#if MOL_BUILD_PROFILE == 1
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.max_voices = 12u;
  config.command_capacity = 32u;
  config.event_capacity = 32u;
  EXPECT_TRUE(mol_engine_query_memory(&config) <= 131072u);
  EXPECT_TRUE(mol_engine_init(storage.bytes, 131072u, &config, &engine) == MOL_OK);
  mol_engine_shutdown(engine);
#endif
}

static void test_planar_and_commands(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t command = {0};
  float left[16] = {1.0f};
  float right[16] = {1.0f};
  float* channels[2] = {left, right};

  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_planar_f32(engine, channels, 16u, 2u) == MOL_OK);
  for (size_t index = 0u; index < 16u; ++index) {
    EXPECT_TRUE(left[index] == 0.0f);
    EXPECT_TRUE(right[index] == 0.0f);
  }

  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = MOL_COMMAND_RESET_ENGINE;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  command.command_type = MOL_COMMAND_SET_TEMPO;
  command.payload.scalar.value = 100.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_lifecycle();
  test_validation();
  test_tiny_embedded_budget();
  test_planar_and_commands();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
