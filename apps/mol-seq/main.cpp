// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "midi.hpp"
#include "operations.hpp"
#include "sequence_document.hpp"

namespace {

void usage(const char* executable) {
  std::fprintf(stderr,
               "Usage:\n"
               "  %s inspect INPUT.molseq\n"
               "  %s validate INPUT.molseq\n"
               "  %s json-to-binary INPUT.molseq.json OUTPUT.molseq\n"
               "  %s binary-to-json INPUT.molseq OUTPUT.molseq.json\n"
               "  %s midi-import INPUT.mid OUTPUT.molseq [--sample-rate HZ]\n"
               "  %s midi-export INPUT.molseq OUTPUT.mid\n"
               "  %s trim INPUT.molseq OUTPUT.molseq START_FRAME END_FRAME\n"
               "  %s merge OUTPUT.molseq INPUT1.molseq INPUT2.molseq [...]\n"
               "  %s quantize INPUT.molseq OUTPUT.molseq GRID_FRAMES\n",
               executable, executable, executable, executable, executable, executable, executable,
               executable, executable);
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

std::uint64_t parse_u64(const char* text, const char* label) {
  if (text[0] == '\0' || text[0] == '-') throw std::runtime_error(std::string("invalid ") + label);
  std::size_t used = 0u;
  const unsigned long long parsed = std::stoull(text, &used, 10);
  if (used == 0u || text[used] != '\0') throw std::runtime_error(std::string("invalid ") + label);
  return static_cast<std::uint64_t>(parsed);
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
    if (command == "midi-import" && (argc == 4 || argc == 6)) {
      std::uint32_t sample_rate = 48000u;
      if (argc == 6) {
        if (std::string(argv[4]) != "--sample-rate")
          throw std::runtime_error("expected --sample-rate HZ");
        const unsigned long parsed = std::stoul(argv[5]);
        if (parsed > std::numeric_limits<std::uint32_t>::max())
          throw std::runtime_error("sample rate is out of range");
        sample_rate = static_cast<std::uint32_t>(parsed);
      }
      molseq::save_binary(argv[3], molseq::import_midi(argv[2], sample_rate));
      return 0;
    }
    if (command == "midi-export" && argc == 4) {
      molseq::export_midi(argv[3], molseq::load_binary(argv[2]));
      return 0;
    }
    if (command == "trim" && argc == 6) {
      molseq::save_binary(argv[3], molseq::trim_sequence(molseq::load_binary(argv[2]),
                                                         parse_u64(argv[4], "start frame"),
                                                         parse_u64(argv[5], "end frame")));
      return 0;
    }
    if (command == "merge" && argc >= 5) {
      std::vector<molseq::SequenceDocument> inputs;
      inputs.reserve(static_cast<std::size_t>(argc - 3));
      for (int index = 3; index < argc; ++index) inputs.push_back(molseq::load_binary(argv[index]));
      molseq::save_binary(argv[2], molseq::merge_sequences(inputs));
      return 0;
    }
    if (command == "quantize" && argc == 5) {
      molseq::save_binary(argv[3], molseq::quantize_sequence(molseq::load_binary(argv[2]),
                                                             parse_u64(argv[4], "quantize grid")));
      return 0;
    }
    usage(argv[0]);
    return 2;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "mol-seq: %s\n", error.what());
    return 1;
  }
}
