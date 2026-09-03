// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_SEQ_MIDI_HPP_
#define MOL_SEQ_MIDI_HPP_

#include <cstddef>
#include <cstdint>
#include <string>

#include "sequence_document.hpp"

namespace molseq {

SequenceDocument import_midi(const std::string& path, std::uint32_t sample_rate);
SequenceDocument import_midi_bytes(const std::uint8_t* data, std::size_t size,
                                   std::uint32_t sample_rate);
void export_midi(const std::string& path, const SequenceDocument& document);

}  // namespace molseq

#endif  // MOL_SEQ_MIDI_HPP_
