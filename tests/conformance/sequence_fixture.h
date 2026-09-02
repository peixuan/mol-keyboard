/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_TEST_SEQUENCE_FIXTURE_H_
#define MOL_TEST_SEQUENCE_FIXTURE_H_

#include <stdint.h>

typedef struct mol_sequence_fixture_summary {
  uint32_t sample_rate;
  uint32_t time_base;
  uint32_t event_count;
  uint32_t metadata_count;
  uint32_t note_on_count;
  uint32_t note_off_count;
  uint32_t transport_start_count;
  uint32_t transport_stop_count;
  uint64_t final_frame;
} mol_sequence_fixture_summary_t;

int mol_sequence_fixture_verify(mol_sequence_fixture_summary_t* summary);

#endif /* MOL_TEST_SEQUENCE_FIXTURE_H_ */
