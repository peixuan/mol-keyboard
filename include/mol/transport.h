/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_TRANSPORT_H_
#define MOL_TRANSPORT_H_

#include <stdint.h>

#include "mol/command.h"
#include "mol/export.h"
#include "mol/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_TEMPO_MIN 30.0f
#define MOL_TEMPO_MAX 300.0f
#define MOL_TEMPO_DEFAULT 100.0f

/** Converts a supported BPM value to the core's deterministic milli-BPM form. */
MOL_API mol_result_t mol_tempo_to_milli_bpm(float tempo, uint32_t* out_milli_bpm);

/** Returns nonzero for the supported 2/4, 3/4, 4/4, 5/4, and 6/8 signatures. */
MOL_API int mol_time_signature_is_valid(uint8_t numerator, uint8_t denominator);

/** Computes an absolute subdivision frame without cumulative rounding drift. */
MOL_API mol_result_t mol_transport_step_frame(uint32_t sample_rate, uint32_t milli_bpm,
                                              uint32_t steps_per_quarter, uint64_t step,
                                              mol_frame_index_t* out_frame);

/** Finds the first subdivision whose computed frame is not before frame. */
MOL_API mol_result_t mol_transport_step_at_or_after(uint32_t sample_rate, uint32_t milli_bpm,
                                                    uint32_t steps_per_quarter,
                                                    mol_frame_index_t frame, uint64_t* out_step);

#ifdef __cplusplus
}
#endif

#endif /* MOL_TRANSPORT_H_ */
