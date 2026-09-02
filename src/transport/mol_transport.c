/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "mol/transport.h"

static mol_result_t mol_transport_ratio(uint32_t sample_rate, uint32_t milli_bpm,
                                        uint32_t steps_per_quarter, uint64_t* numerator,
                                        uint64_t* denominator) {
  if (numerator == NULL || denominator == NULL || sample_rate == 0u || milli_bpm == 0u ||
      steps_per_quarter == 0u) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *numerator = (uint64_t)sample_rate * UINT64_C(60000);
  *denominator = (uint64_t)milli_bpm * steps_per_quarter;
  return MOL_OK;
}

mol_result_t mol_tempo_to_milli_bpm(float tempo, uint32_t* out_milli_bpm) {
  if (out_milli_bpm == NULL || !isfinite(tempo) || tempo < MOL_TEMPO_MIN || tempo > MOL_TEMPO_MAX) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *out_milli_bpm = (uint32_t)(tempo * 1000.0f + 0.5f);
  return MOL_OK;
}

int mol_time_signature_is_valid(uint8_t numerator, uint8_t denominator) {
  if (denominator == 4u && numerator >= 2u && numerator <= 5u) {
    return 1;
  }
  return denominator == 8u && numerator == 6u;
}

mol_result_t mol_transport_step_frame(uint32_t sample_rate, uint32_t milli_bpm,
                                      uint32_t steps_per_quarter, uint64_t step,
                                      mol_frame_index_t* out_frame) {
  uint64_t numerator;
  uint64_t denominator;
  uint64_t whole;
  uint64_t remainder;
  uint64_t frame;
  mol_result_t result;
  if (out_frame == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  result = mol_transport_ratio(sample_rate, milli_bpm, steps_per_quarter, &numerator, &denominator);
  if (result != MOL_OK) {
    return result;
  }
  whole = step / denominator;
  remainder = step % denominator;
  if (whole > UINT64_MAX / numerator) {
    return MOL_ERROR_OVERFLOW;
  }
  frame = whole * numerator;
  if (remainder != 0u) {
    uint64_t fractional;
    if (remainder > UINT64_MAX / numerator) {
      return MOL_ERROR_OVERFLOW;
    }
    fractional = (remainder * numerator) / denominator;
    if (frame > UINT64_MAX - fractional) {
      return MOL_ERROR_OVERFLOW;
    }
    frame += fractional;
  }
  *out_frame = frame;
  return MOL_OK;
}

mol_result_t mol_transport_step_at_or_after(uint32_t sample_rate, uint32_t milli_bpm,
                                            uint32_t steps_per_quarter, mol_frame_index_t frame,
                                            uint64_t* out_step) {
  uint64_t numerator;
  uint64_t denominator;
  uint64_t whole;
  uint64_t remainder;
  uint64_t step;
  mol_result_t result;
  if (out_step == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  result = mol_transport_ratio(sample_rate, milli_bpm, steps_per_quarter, &numerator, &denominator);
  if (result != MOL_OK) {
    return result;
  }
  whole = frame / numerator;
  remainder = frame % numerator;
  if (whole > UINT64_MAX / denominator) {
    return MOL_ERROR_OVERFLOW;
  }
  step = whole * denominator;
  if (remainder != 0u) {
    uint64_t partial;
    uint64_t product;
    if (remainder > UINT64_MAX / denominator) {
      return MOL_ERROR_OVERFLOW;
    }
    product = remainder * denominator;
    partial = product / numerator + (product % numerator != 0u ? 1u : 0u);
    if (step > UINT64_MAX - partial) {
      return MOL_ERROR_OVERFLOW;
    }
    step += partial;
  }
  *out_step = step;
  return MOL_OK;
}
