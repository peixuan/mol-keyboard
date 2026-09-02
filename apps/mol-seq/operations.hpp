// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_SEQ_OPERATIONS_HPP_
#define MOL_SEQ_OPERATIONS_HPP_

#include <cstdint>
#include <vector>

#include "sequence_document.hpp"

namespace molseq {

SequenceDocument trim_sequence(const SequenceDocument& input, std::uint64_t start_frame,
                               std::uint64_t end_frame);
SequenceDocument merge_sequences(const std::vector<SequenceDocument>& inputs);
SequenceDocument quantize_sequence(const SequenceDocument& input, std::uint64_t grid_frames);

}  // namespace molseq

#endif  // MOL_SEQ_OPERATIONS_HPP_
