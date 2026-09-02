// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <exception>
#include <string>

#include "sequence_document.hpp"

namespace {

void usage(const char* executable) {
  std::fprintf(stderr,
               "Usage:\n"
               "  %s inspect INPUT.molseq\n"
               "  %s validate INPUT.molseq\n"
               "  %s json-to-binary INPUT.molseq.json OUTPUT.molseq\n"
               "  %s binary-to-json INPUT.molseq OUTPUT.molseq.json\n",
               executable, executable, executable, executable);
}

int inspect(const std::string& path) {
  const molseq::SequenceDocument document = molseq::load_binary(path);
  const std::uint64_t final_frame = document.events.empty() ? 0u : document.events.back().frame;
  std::printf("format=Mol Sequence v%u\n", MOL_SEQUENCE_FORMAT_VERSION);
  std::printf("sample_rate=%u\n", document.config.sample_rate);
  std::printf("time_base=%u\n", document.config.time_base);
  std::printf("preset=%u\n", document.config.initial_state.preset);
  std::printf("tempo=%.3f\n", static_cast<double>(document.config.initial_state.tempo));
  std::printf("events=%zu\n", document.events.size());
  std::printf("metadata_chunks=%zu\n", document.metadata.size());
  std::printf("final_frame=%llu\n", static_cast<unsigned long long>(final_frame));
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    usage(argv[0]);
    return 2;
  }
  try {
    const std::string command = argv[1];
    if (command == "inspect" && argc == 3) return inspect(argv[2]);
    if (command == "validate" && argc == 3) {
      const molseq::SequenceDocument document = molseq::load_binary(argv[2]);
      std::printf("valid: %zu events, %zu metadata chunks\n", document.events.size(),
                  document.metadata.size());
      return 0;
    }
    if (command == "json-to-binary" && argc == 4) {
      molseq::save_binary(argv[3], molseq::load_json(argv[2]));
      return 0;
    }
    if (command == "binary-to-json" && argc == 4) {
      molseq::save_json(argv[3], molseq::load_binary(argv[2]));
      return 0;
    }
    usage(argv[0]);
    return 2;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "mol-seq: %s\n", error.what());
    return 1;
  }
}
