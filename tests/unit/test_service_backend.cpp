// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "service_backend.hpp"

namespace {

constexpr std::size_t kEngineMemoryBytes = 1048576u;
std::vector<std::string> exercised_methods;

struct EngineMemory {
  alignas(std::max_align_t) std::array<unsigned char, kEngineMemoryBytes> bytes{};
};

class TestRuntime final : public molcontrol::ServiceRuntime {
 public:
  TestRuntime() {
    mol_engine_config_t config = mol_engine_config_default();
    if (mol_engine_init(memory_->bytes.data(), memory_->bytes.size(), &config, &engine_) != MOL_OK)
      throw std::runtime_error("engine initialization failed");
  }

  ~TestRuntime() override { mol_engine_shutdown(engine_); }

  mol_result_t submit(const mol_command_t& command) override {
    commands.push_back(command.command_type);
    const mol_result_t result = mol_engine_submit(engine_, &command);
    if (result != MOL_OK) return result;
    std::array<float, 128u> output{};
    return mol_engine_render_interleaved_f32(engine_, output.data(), 64u, 2u);
  }

  mol_result_t snapshot(mol_engine_state_t& state) override {
    return mol_engine_get_state(engine_, &state);
  }

  mol_capability_flags_t capabilities() const override {
    return mol_engine_get_capabilities(engine_);
  }

  molcontrol::RuntimeMetrics metrics() const override {
    molcontrol::RuntimeMetrics result;
    result.callbacks = 4u;
    result.rendered_frames = 256u;
    return result;
  }

  std::vector<molcontrol::DeviceInfo> input_devices() override {
    return {{"keyboard:default", "Default keyboard", "test", true, input_id_ == "keyboard:default",
             false}};
  }

  mol_result_t attach_input(const std::string& id) override {
    if (id != "keyboard:default") return MOL_ERROR_INVALID_ARGUMENT;
    input_id_ = id;
    return MOL_OK;
  }

  mol_result_t detach_input() override {
    input_id_.clear();
    return MOL_OK;
  }

  std::string active_input_id() const override { return input_id_; }

  std::vector<molcontrol::DeviceInfo> output_devices() override {
    return {
        {"default", "Default output", "test", true, output_id_ == "default", false},
        {"bluetooth", "Test Bluetooth speaker", "test", false, output_id_ == "bluetooth", true}};
  }

  mol_result_t select_output(const std::string& id) override {
    if (id != "default" && id != "bluetooth") return MOL_ERROR_INVALID_ARGUMENT;
    output_id_ = id;
    return MOL_OK;
  }

  molcontrol::AudioStatus audio_status() const override {
    molcontrol::AudioStatus result;
    result.available = true;
    result.backend = "test";
    result.channel_count = 2u;
    result.device_id = output_id_;
    result.device_name = "Test output";
    result.estimated_latency_ms = 8.0;
    result.low_latency_requested = true;
    result.period_frames = 128u;
    result.periods = 3u;
    result.sample_rate = 48000u;
    return result;
  }

  mol_result_t load_sequence(const molseq::SequenceDocument& document) override {
    return mol_engine_load_sequence(engine_, &document.config, document.events.data(),
                                    static_cast<std::uint32_t>(document.events.size()));
  }

  mol_result_t copy_recording(molseq::SequenceDocument& document) override {
    document.events.resize(2048u);
    std::uint32_t count = 0u;
    const mol_result_t result =
        mol_engine_copy_recording(engine_, &document.config, document.events.data(),
                                  static_cast<std::uint32_t>(document.events.size()), &count);
    document.events.resize(count);
    return result;
  }

  bool runtime_self_test(std::string& detail) override {
    detail = "test runtime is healthy";
    return true;
  }

  mol_result_t benchmark(std::uint64_t frames, molcontrol::BenchmarkResult& result) override {
    result.frames = frames;
    result.elapsed_ms = 1.0;
    result.realtime_ratio = 100.0;
    result.peak = 0.25;
    return MOL_OK;
  }

  void request_shutdown() override { shutdown_requested = true; }

  std::vector<mol_command_type_t> commands;
  bool shutdown_requested = false;

 private:
  std::unique_ptr<EngineMemory> memory_ = std::make_unique<EngineMemory>();
  mol_engine_t* engine_ = nullptr;
  std::string input_id_;
  std::string output_id_ = "default";
};

molseq::Json dispatch(molcontrol::JsonRpcDispatcher& dispatcher, const std::string& method,
                      const std::string& params = "{}") {
  static std::uint64_t id = 1u;
  const std::string request = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method +
                              "\",\"params\":" + params + ",\"id\":" + std::to_string(id++) + "}";
  const molseq::Json response = molseq::parse_json(dispatcher.dispatch(request));
  const molseq::Json* error = molseq::optional_member(response, "error");
  if (error != nullptr)
    throw std::runtime_error("unexpected RPC error for " + method + ": " +
                             molseq::write_json(*error));
  exercised_methods.push_back(method);
  return molseq::require_member(response, "result");
}

bool dispatch_fails(molcontrol::JsonRpcDispatcher& dispatcher, const std::string& method,
                    const std::string& params) {
  const std::string request =
      "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params + ",\"id\":99}";
  const molseq::Json response = molseq::parse_json(dispatcher.dispatch(request));
  return molseq::optional_member(response, "error") != nullptr;
}

}  // namespace

int main() {
  try {
    const std::filesystem::path state =
        std::filesystem::temp_directory_path() /
        ("mol-service-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
      std::filesystem::path path;
      ~Cleanup() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
      }
    } cleanup{state};
    auto runtime = std::make_unique<TestRuntime>();
    molcontrol::ServiceBackend backend(*runtime, state);
    molcontrol::JsonRpcDispatcher dispatcher;
    if (!molcontrol::register_service_methods(dispatcher, backend)) return 1;

    dispatch(dispatcher, "system.getInfo");
    dispatch(dispatcher, "system.getCapabilities");
    dispatch(dispatcher, "system.getMetrics");
    dispatch(dispatcher, "engine.getState");
    dispatch(dispatcher, "engine.reset");
    dispatch(dispatcher, "engine.allNotesOff");
    dispatch(dispatcher, "engine.allSoundOff");
    dispatch(dispatcher, "preset.list");
    dispatch(dispatcher, "preset.select", "{\"preset\":\"electric-piano\"}");
    dispatch(dispatcher, "preset.getParameters");
    dispatch(dispatcher, "preset.setParameter", "{\"parameter\":3,\"value\":0.25}");
    dispatch(dispatcher, "transport.get");
    dispatch(dispatcher, "transport.setTempo", "{\"bpm\":120}");
    dispatch(dispatcher, "transport.setTimeSignature", "{\"numerator\":6,\"denominator\":8}");
    dispatch(dispatcher, "transport.start");
    dispatch(dispatcher, "transport.stop");
    dispatch(dispatcher, "input.listDevices");
    dispatch(dispatcher, "input.attach", "{\"id\":\"keyboard:default\"}");
    dispatch(dispatcher, "input.getMapping");
    dispatch(dispatcher, "input.setMapping", "{\"mapping\":{\"4\":60}}");
    dispatch(dispatcher, "input.detach");
    dispatch(dispatcher, "audio.listDevices");
    dispatch(dispatcher, "audio.selectDevice", "{\"id\":\"bluetooth\"}");
    dispatch(dispatcher, "audio.getLatency");
    dispatch(dispatcher, "recording.start");
    dispatch(dispatcher, "performance.noteOn", "{\"note\":60,\"velocity\":0.8,\"gesture\":42}");
    dispatch(dispatcher, "performance.control", "{\"control\":\"sustain\",\"value\":1}");
    dispatch(dispatcher, "performance.noteOff", "{\"gesture\":42}");
    dispatch(dispatcher, "recording.stop", "{\"name\":\"take.molseq\"}");
    const molseq::Json recordings = dispatch(dispatcher, "recording.list");
    if (recordings.type != molseq::Json::Type::Array || recordings.array.size() != 1u) return 1;
    dispatch(dispatcher, "recording.save", "{\"name\":\"copy.molseq\"}");
    dispatch(dispatcher, "recording.load", "{\"name\":\"take.molseq\"}");
    dispatch(dispatcher, "playback.start", "{\"name\":\"take.molseq\"}");
    dispatch(dispatcher, "playback.seek", "{\"frame\":0}");
    dispatch(dispatcher, "playback.stop");
    dispatch(dispatcher, "config.get");
    dispatch(dispatcher, "config.set", "{\"key\":\"log_level\",\"value\":\"debug\"}");
    dispatch(dispatcher, "diagnostics.selfTest");
    dispatch(dispatcher, "diagnostics.doctor");
    dispatch(dispatcher, "diagnostics.benchmark", "{\"frames\":1024}");
    dispatch(dispatcher, "system.shutdown");

    if (!runtime->shutdown_requested ||
        !std::filesystem::is_regular_file(backend.recordings_directory() / "take.molseq") ||
        !dispatch_fails(dispatcher, "recording.save", "{\"name\":\"../escape.molseq\"}") ||
        !dispatch_fails(dispatcher, "transport.setTempo", "{\"bpm\":500}") ||
        !dispatch_fails(dispatcher, "engine.getState", "{\"unknown\":true}")) {
      return 1;
    }

    for (const std::string_view method : molcontrol::kRequiredRpcMethods)
      if (std::find(exercised_methods.begin(), exercised_methods.end(), method) ==
          exercised_methods.end())
        return 1;
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Service backend test failed: %s\n", error.what());
    return 1;
  }
}
