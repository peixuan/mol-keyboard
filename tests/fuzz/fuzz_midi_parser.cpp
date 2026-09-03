// SPDX-License-Identifier: Apache-2.0
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include "midi.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > 1048576u) return 0;
  try {
    const molseq::SequenceDocument first = molseq::import_midi_bytes(data, size, 48000u);
    if (mol_sequence_validate_config(&first.config) != MOL_OK ||
        first.events.size() > MOL_SEQUENCE_MAX_EVENTS) {
      __builtin_trap();
    }
    mol_frame_index_t previous_frame = 0u;
    for (const mol_sequence_event_t& event : first.events) {
      if (event.frame < previous_frame || mol_sequence_validate_event(&event) != MOL_OK)
        __builtin_trap();
      previous_frame = event.frame;
    }
    const molseq::SequenceDocument second = molseq::import_midi_bytes(data, size, 48000u);
    const std::string first_json = molseq::write_json(molseq::document_to_json(first));
    const std::string second_json = molseq::write_json(molseq::document_to_json(second));
    if (first_json != second_json) __builtin_trap();
  } catch (const std::exception&) {
  }
  return 0;
}
