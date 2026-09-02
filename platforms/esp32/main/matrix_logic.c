/* SPDX-License-Identifier: Apache-2.0 */
#include "matrix_logic.h"

#include <string.h>

static uint32_t key_mask(uint32_t key_count) {
  return key_count == 32u ? UINT32_MAX : (UINT32_C(1) << key_count) - UINT32_C(1);
}

static uint32_t row_bits(uint32_t bits, uint32_t row, uint32_t columns) {
  return (bits >> (row * columns)) & key_mask(columns);
}

static uint32_t ambiguous_keys(const mol_matrix_state_t* state, uint32_t raw_bits) {
  const uint32_t rows = state->config.rows;
  const uint32_t columns = state->config.columns;
  uint32_t ambiguous = 0u;
  uint32_t first_row;

  for (first_row = 0u; first_row < rows; ++first_row) {
    uint32_t second_row;
    const uint32_t first = row_bits(raw_bits, first_row, columns);
    for (second_row = first_row + 1u; second_row < rows; ++second_row) {
      const uint32_t common = first & row_bits(raw_bits, second_row, columns);
      if ((common & (common - UINT32_C(1))) != 0u) {
        ambiguous |= common << (first_row * columns);
        ambiguous |= common << (second_row * columns);
      }
    }
  }
  return ambiguous;
}

mol_matrix_result_t mol_matrix_init(mol_matrix_state_t* state, const mol_matrix_config_t* config) {
  uint32_t key_count;
  if (state == NULL || config == NULL || config->rows == 0u || config->rows > MOL_MATRIX_MAX_ROWS ||
      config->columns == 0u || config->columns > MOL_MATRIX_MAX_COLUMNS ||
      config->debounce_scans == 0u || config->config_hold_scans == 0u ||
      (config->ghost_policy != MOL_MATRIX_GHOST_ALLOW &&
       config->ghost_policy != MOL_MATRIX_GHOST_SUPPRESS_AMBIGUOUS)) {
    return MOL_MATRIX_INVALID_ARGUMENT;
  }
  key_count = (uint32_t)config->rows * (uint32_t)config->columns;
  if ((uint32_t)config->config_key >= key_count) {
    return MOL_MATRIX_INVALID_ARGUMENT;
  }
  memset(state, 0, sizeof(*state));
  state->config = *config;
  return MOL_MATRIX_OK;
}

mol_matrix_result_t mol_matrix_process(mol_matrix_state_t* state, uint32_t raw_bits,
                                       mol_matrix_event_t* events, size_t event_capacity,
                                       size_t* event_count, bool* ghost_detected) {
  mol_matrix_state_t next;
  mol_matrix_event_t pending[MOL_MATRIX_MAX_EVENTS];
  const uint32_t key_count =
      state != NULL ? (uint32_t)state->config.rows * (uint32_t)state->config.columns : 0u;
  uint32_t ambiguous = 0u;
  size_t pending_count = 0u;
  uint32_t key;

  if (state == NULL || event_count == NULL || ghost_detected == NULL ||
      (events == NULL && event_capacity != 0u) || key_count == 0u ||
      key_count > MOL_MATRIX_MAX_KEYS) {
    return MOL_MATRIX_INVALID_ARGUMENT;
  }
  next = *state;
  raw_bits &= key_mask(key_count);
  if (state->config.ghost_policy == MOL_MATRIX_GHOST_SUPPRESS_AMBIGUOUS) {
    ambiguous = ambiguous_keys(state, raw_bits);
    raw_bits = (raw_bits & ~ambiguous) | (state->stable_bits & ambiguous);
  }
  *ghost_detected = ambiguous != 0u;

  for (key = 0u; key < key_count; ++key) {
    const uint32_t bit = UINT32_C(1) << key;
    const bool requested = (raw_bits & bit) != 0u;
    const bool stable = (next.stable_bits & bit) != 0u;
    const bool candidate = (next.candidate_bits & bit) != 0u;
    if (requested == stable) {
      next.candidate_age[key] = 0u;
      if (requested) {
        next.candidate_bits |= bit;
      } else {
        next.candidate_bits &= ~bit;
      }
      continue;
    }
    if (requested != candidate) {
      if (requested) {
        next.candidate_bits |= bit;
      } else {
        next.candidate_bits &= ~bit;
      }
      next.candidate_age[key] = 1u;
    } else if (next.candidate_age[key] < UINT16_MAX) {
      ++next.candidate_age[key];
    }
    if (next.candidate_age[key] >= next.config.debounce_scans) {
      if (requested) {
        next.stable_bits |= bit;
      } else {
        next.stable_bits &= ~bit;
      }
      next.candidate_age[key] = 0u;
      pending[pending_count].type = requested ? MOL_MATRIX_EVENT_KEY_DOWN : MOL_MATRIX_EVENT_KEY_UP;
      pending[pending_count].key = (uint8_t)key;
      ++pending_count;
    }
  }

  if ((next.stable_bits & (UINT32_C(1) << next.config.config_key)) != 0u) {
    if (!next.config_hold_fired) {
      if (next.config_hold_age < UINT16_MAX) {
        ++next.config_hold_age;
      }
      if (next.config_hold_age >= next.config.config_hold_scans) {
        next.config_hold_fired = true;
        pending[pending_count].type = MOL_MATRIX_EVENT_CONFIG_HOLD;
        pending[pending_count].key = next.config.config_key;
        ++pending_count;
      }
    }
  } else {
    next.config_hold_age = 0u;
    next.config_hold_fired = false;
  }

  *event_count = pending_count;
  if (pending_count > event_capacity) {
    return MOL_MATRIX_BUFFER_TOO_SMALL;
  }
  if (pending_count != 0u) {
    memcpy(events, pending, pending_count * sizeof(*events));
  }
  *state = next;
  return MOL_MATRIX_OK;
}
