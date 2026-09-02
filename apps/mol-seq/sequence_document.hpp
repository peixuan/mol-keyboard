// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_SEQ_SEQUENCE_DOCUMENT_HPP_
#define MOL_SEQ_SEQUENCE_DOCUMENT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"

extern "C" {
#include "mol/mol.h"
}

namespace molseq {

struct Metadata {
  std::uint32_t type = 0u;
  std::vector<std::uint8_t> data;
};

struct SequenceDocument {
  mol_sequence_config_t config = mol_sequence_config_default(48000u);
  std::vector<mol_sequence_event_t> events;
  std::vector<Metadata> metadata;
};

SequenceDocument load_binary(const std::string& path);
void save_binary(const std::string& path, const SequenceDocument& document);
SequenceDocument load_json(const std::string& path);
void save_json(const std::string& path, const SequenceDocument& document);
Json document_to_json(const SequenceDocument& document);
SequenceDocument document_from_json(const Json& root);
const char* command_name(mol_command_type_t type);
mol_command_type_t command_type(const std::string& name);

}  // namespace molseq

#endif  // MOL_SEQ_SEQUENCE_DOCUMENT_HPP_
