// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_KEYBOARDD_AUDIO_RUNTIME_HPP_
#define MOL_KEYBOARDD_AUDIO_RUNTIME_HPP_

#include <cstddef>
#include <memory>
#include <string>

#include "mol/event.h"
#include "service_runtime.hpp"

namespace molkeyboardd {

class AudioRuntime final : public molcontrol::ServiceRuntime {
 public:
  AudioRuntime();
  ~AudioRuntime() override;
  AudioRuntime(const AudioRuntime&) = delete;
  AudioRuntime& operator=(const AudioRuntime&) = delete;

  bool start(bool null_backend, const std::string& device_id, std::string& error);
  void stop();

  mol_result_t submit(const mol_command_t& command) override;
  mol_result_t snapshot(mol_engine_state_t& state) override;
  mol_capability_flags_t capabilities() const override;
  molcontrol::RuntimeMetrics metrics() const override;
  bool midi_supported() const override;
  std::vector<molcontrol::DeviceInfo> input_devices() override;
  mol_result_t attach_input(const std::string& id) override;
  mol_result_t detach_input() override;
  std::string active_input_id() const override;
  std::vector<molcontrol::DeviceInfo> output_devices() override;
  mol_result_t select_output(const std::string& id) override;
  molcontrol::AudioStatus audio_status() const override;
  mol_result_t load_sequence(const molseq::SequenceDocument& document) override;
  mol_result_t copy_recording(molseq::SequenceDocument& document) override;
  bool runtime_self_test(std::string& detail) override;
  mol_result_t benchmark(std::uint64_t frames, molcontrol::BenchmarkResult& result) override;
  void request_shutdown() override;

  [[nodiscard]] bool shutdown_requested() const;
  std::size_t poll_events(mol_event_t* events, std::size_t capacity) noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace molkeyboardd

#endif  // MOL_KEYBOARDD_AUDIO_RUNTIME_HPP_
