// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_CONTROL_SERVICE_RUNTIME_HPP_
#define MOL_CONTROL_SERVICE_RUNTIME_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "sequence_document.hpp"

namespace molcontrol {

struct DeviceInfo {
  std::string id;
  std::string name;
  std::string backend;
  bool is_default = false;
  bool is_active = false;
  bool is_bluetooth = false;
  bool is_physical_input = false;
};

struct AudioStatus {
  std::string backend;
  std::string device_id;
  std::string device_name;
  std::uint32_t sample_rate = 0u;
  std::uint32_t channel_count = 0u;
  std::uint32_t period_frames = 0u;
  std::uint32_t periods = 0u;
  double estimated_latency_ms = 0.0;
  bool available = false;
  bool null_sink = false;
  bool low_latency_requested = false;
};

struct RuntimeMetrics {
  std::uint64_t callbacks = 0u;
  std::uint64_t rendered_frames = 0u;
  std::uint64_t render_failures = 0u;
  std::uint64_t non_finite_samples = 0u;
  std::uint64_t underruns = 0u;
  std::uint64_t dropped_commands = 0u;
  std::uint64_t device_notifications = 0u;
  std::uint64_t device_reroutes = 0u;
  std::uint64_t input_events = 0u;
};

struct BenchmarkResult {
  std::uint64_t frames = 0u;
  double elapsed_ms = 0.0;
  double realtime_ratio = 0.0;
  double peak = 0.0;
  std::uint64_t non_finite_samples = 0u;
};

class ServiceRuntime {
 public:
  virtual ~ServiceRuntime() = default;

  virtual mol_result_t submit(const mol_command_t& command) = 0;
  virtual mol_result_t snapshot(mol_engine_state_t& state) = 0;
  virtual mol_capability_flags_t capabilities() const = 0;
  virtual RuntimeMetrics metrics() const = 0;

  virtual std::vector<DeviceInfo> input_devices() = 0;
  virtual mol_result_t attach_input(const std::string& id) = 0;
  virtual mol_result_t detach_input() = 0;
  virtual std::string active_input_id() const = 0;

  virtual std::vector<DeviceInfo> output_devices() = 0;
  virtual mol_result_t select_output(const std::string& id) = 0;
  virtual AudioStatus audio_status() const = 0;

  virtual mol_result_t load_sequence(const molseq::SequenceDocument& document) = 0;
  virtual mol_result_t copy_recording(molseq::SequenceDocument& document) = 0;
  virtual bool runtime_self_test(std::string& detail) = 0;
  virtual mol_result_t benchmark(std::uint64_t frames, BenchmarkResult& result) = 0;
  virtual void request_shutdown() = 0;
};

}  // namespace molcontrol

#endif  // MOL_CONTROL_SERVICE_RUNTIME_HPP_
