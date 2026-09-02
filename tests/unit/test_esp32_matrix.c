/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "matrix_logic.h"

static int failures;

#define EXPECT_TRUE(condition)                                                           \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      fprintf(stderr, "%s:%d expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static mol_matrix_config_t test_config(void) {
  mol_matrix_config_t config;
  memset(&config, 0, sizeof(config));
  config.rows = 5u;
  config.columns = 6u;
  config.config_key = 29u;
  config.clear_pairing_key = 28u;
  config.factory_reset_key = 27u;
  config.ghost_policy = MOL_MATRIX_GHOST_SUPPRESS_AMBIGUOUS;
  config.debounce_scans = 3u;
  config.config_hold_scans = 4u;
  config.clear_pairing_hold_scans = 5u;
  config.factory_reset_hold_scans = 6u;
  return config;
}

static size_t process(mol_matrix_state_t* state, uint32_t bits, mol_matrix_event_t* events,
                      bool* ghost) {
  size_t count = 0u;
  EXPECT_TRUE(mol_matrix_process(state, bits, events, MOL_MATRIX_MAX_EVENTS, &count, ghost) ==
              MOL_MATRIX_OK);
  return count;
}

static void test_validation(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  config.debounce_scans = 0u;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_INVALID_ARGUMENT);
  config = test_config();
  config.config_key = 30u;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_INVALID_ARGUMENT);
  config = test_config();
  config.clear_pairing_key = config.config_key;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_INVALID_ARGUMENT);
}

static void test_debounce_and_order(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
  bool ghost = false;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  EXPECT_TRUE(process(&state, UINT32_C(1) << 2u, events, &ghost) == 0u);
  EXPECT_TRUE(process(&state, UINT32_C(1) << 2u, events, &ghost) == 0u);
  EXPECT_TRUE(process(&state, UINT32_C(1) << 2u, events, &ghost) == 1u);
  EXPECT_TRUE(events[0].type == MOL_MATRIX_EVENT_KEY_DOWN && events[0].key == 2u);
  EXPECT_TRUE(process(&state, 0u, events, &ghost) == 0u);
  EXPECT_TRUE(process(&state, UINT32_C(1) << 2u, events, &ghost) == 0u);
  EXPECT_TRUE(process(&state, 0u, events, &ghost) == 0u);
  EXPECT_TRUE(process(&state, 0u, events, &ghost) == 0u);
  EXPECT_TRUE(process(&state, 0u, events, &ghost) == 1u);
  EXPECT_TRUE(events[0].type == MOL_MATRIX_EVENT_KEY_UP && events[0].key == 2u);
}

static void test_ghost_suppression_freezes_stable_keys(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
  bool ghost = false;
  const uint32_t existing = (UINT32_C(1) << 0u) | (UINT32_C(1) << 6u);
  const uint32_t rectangle = existing | (UINT32_C(1) << 1u) | (UINT32_C(1) << 7u);
  uint32_t scan;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  for (scan = 0u; scan < 3u; ++scan) {
    (void)process(&state, existing, events, &ghost);
  }
  EXPECT_TRUE(state.stable_bits == existing);
  for (scan = 0u; scan < 6u; ++scan) {
    EXPECT_TRUE(process(&state, rectangle, events, &ghost) == 0u);
    EXPECT_TRUE(ghost);
  }
  EXPECT_TRUE(state.stable_bits == existing);
}

static void test_diode_policy_allows_rectangle(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
  bool ghost = true;
  size_t count = 0u;
  uint32_t scan;
  const uint32_t rectangle =
      (UINT32_C(1) << 0u) | (UINT32_C(1) << 1u) | (UINT32_C(1) << 6u) | (UINT32_C(1) << 7u);
  config.ghost_policy = MOL_MATRIX_GHOST_ALLOW;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  for (scan = 0u; scan < 3u; ++scan) {
    count = process(&state, rectangle, events, &ghost);
  }
  EXPECT_TRUE(!ghost);
  EXPECT_TRUE(count == 4u);
  EXPECT_TRUE(events[0].key == 0u && events[1].key == 1u && events[2].key == 6u &&
              events[3].key == 7u);
}

static void test_config_hold_fires_once(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
  bool ghost = false;
  size_t count;
  uint32_t scan;
  const uint32_t key = UINT32_C(1) << 29u;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  for (scan = 0u; scan < 3u; ++scan) {
    (void)process(&state, key, events, &ghost);
  }
  for (scan = 0u; scan < 2u; ++scan) {
    EXPECT_TRUE(process(&state, key, events, &ghost) == 0u);
  }
  count = process(&state, key, events, &ghost);
  EXPECT_TRUE(count == 1u && events[0].type == MOL_MATRIX_EVENT_CONFIG_HOLD &&
              events[0].key == 29u);
  EXPECT_TRUE(process(&state, key, events, &ghost) == 0u);
  for (scan = 0u; scan < 3u; ++scan) {
    (void)process(&state, 0u, events, &ghost);
  }
  EXPECT_TRUE(!state.config_hold_fired);
}

static void test_clear_pairing_chord_is_mutually_exclusive(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
  bool ghost = false;
  size_t count = 0u;
  uint32_t scan;
  const uint32_t chord = (UINT32_C(1) << 29u) | (UINT32_C(1) << 28u);
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  for (scan = 0u; scan < 7u; ++scan) {
    count = process(&state, chord, events, &ghost);
  }
  EXPECT_TRUE(count == 1u && events[0].type == MOL_MATRIX_EVENT_CLEAR_PAIRING_HOLD &&
              events[0].key == 28u);
  EXPECT_TRUE(!state.config_hold_fired && state.clear_pairing_hold_fired);
  EXPECT_TRUE(process(&state, chord, events, &ghost) == 0u);
}

static void test_factory_reset_chord_takes_precedence(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
  bool ghost = false;
  size_t count = 0u;
  uint32_t scan;
  const uint32_t chord = (UINT32_C(1) << 29u) | (UINT32_C(1) << 28u) | (UINT32_C(1) << 27u);
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  for (scan = 0u; scan < 8u; ++scan) {
    count = process(&state, chord, events, &ghost);
  }
  EXPECT_TRUE(count == 1u && events[0].type == MOL_MATRIX_EVENT_FACTORY_RESET_HOLD &&
              events[0].key == 27u);
  EXPECT_TRUE(!state.config_hold_fired && !state.clear_pairing_hold_fired &&
              state.factory_reset_hold_fired);
  EXPECT_TRUE(process(&state, chord, events, &ghost) == 0u);
}

static void test_small_output_is_transactional(void) {
  mol_matrix_state_t state;
  mol_matrix_config_t config = test_config();
  mol_matrix_event_t event;
  bool ghost = false;
  size_t count = 0u;
  uint32_t scan;
  EXPECT_TRUE(mol_matrix_init(&state, &config) == MOL_MATRIX_OK);
  for (scan = 0u; scan < 2u; ++scan) {
    EXPECT_TRUE(mol_matrix_process(&state, UINT32_C(3), &event, 1u, &count, &ghost) ==
                MOL_MATRIX_OK);
  }
  EXPECT_TRUE(mol_matrix_process(&state, UINT32_C(3), &event, 1u, &count, &ghost) ==
              MOL_MATRIX_BUFFER_TOO_SMALL);
  EXPECT_TRUE(count == 2u && state.stable_bits == 0u);
}

int main(void) {
  test_validation();
  test_debounce_and_order();
  test_ghost_suppression_freezes_stable_keys();
  test_diode_policy_allows_rectangle();
  test_config_hold_fires_once();
  test_clear_pairing_chord_is_mutually_exclusive();
  test_factory_reset_chord_takes_precedence();
  test_small_output_is_transactional();
  if (failures != 0) {
    fprintf(stderr, "%d ESP32 matrix test(s) failed\n", failures);
    return 1;
  }
  puts("ESP32 matrix tests passed");
  return 0;
}
