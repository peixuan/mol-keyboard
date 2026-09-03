// SPDX-License-Identifier: Apache-2.0
#include "service_backend.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace molcontrol {
namespace {

using Json = molseq::Json;

constexpr int kInvalidParams = -32602;
constexpr int kRuntimeError = -32000;
constexpr std::uint32_t kServiceSourceId = 1u;
constexpr std::uint64_t kMaximumBenchmarkFrames = UINT64_C(1920000);
constexpr std::uintmax_t kMaximumConfigBytes = 65536u;

void require_object(const Json& value) {
  if (value.type != Json::Type::Object) throw RpcError(kInvalidParams, "params must be an object");
}

void allow_members(const Json& value, std::initializer_list<std::string_view> allowed) {
  require_object(value);
  for (const auto& member : value.object) {
    const bool accepted = std::find(allowed.begin(), allowed.end(), member.first) != allowed.end();
    if (!accepted) throw RpcError(kInvalidParams, "unknown parameter: " + member.first);
  }
}

const Json& required(const Json& params, const char* name) {
  const Json* value = molseq::optional_member(params, name);
  if (value == nullptr) throw RpcError(kInvalidParams, std::string("missing parameter: ") + name);
  return *value;
}

std::string string_value(const Json& value, const char* name, std::size_t maximum = 256u) {
  if (value.type != Json::Type::String || value.text.empty() || value.text.size() > maximum)
    throw RpcError(kInvalidParams, std::string("invalid string parameter: ") + name);
  return value.text;
}

std::uint64_t u64_value(const Json& value, const char* name, std::uint64_t maximum) {
  if (value.type != Json::Type::Number || value.text.empty())
    throw RpcError(kInvalidParams, std::string("invalid integer parameter: ") + name);
  for (const char character : value.text)
    if (character < '0' || character > '9')
      throw RpcError(kInvalidParams, std::string("invalid integer parameter: ") + name);
  std::uint64_t parsed = 0u;
  const auto conversion =
      std::from_chars(value.text.data(), value.text.data() + value.text.size(), parsed);
  if (conversion.ec != std::errc{} || conversion.ptr != value.text.data() + value.text.size() ||
      parsed > maximum)
    throw RpcError(kInvalidParams, std::string("invalid integer parameter: ") + name);
  return parsed;
}

double real_value(const Json& value, const char* name, double minimum, double maximum) {
  if (value.type != Json::Type::Number || value.text.empty())
    throw RpcError(kInvalidParams, std::string("invalid numeric parameter: ") + name);
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(value.text.c_str(), &end);
  if (errno != 0 || end != value.text.data() + value.text.size() || !std::isfinite(parsed) ||
      parsed < minimum || parsed > maximum)
    throw RpcError(kInvalidParams, std::string("invalid numeric parameter: ") + name);
  return parsed;
}

bool bool_value(const Json& value, const char* name) {
  if (value.type != Json::Type::Boolean)
    throw RpcError(kInvalidParams, std::string("invalid Boolean parameter: ") + name);
  return value.boolean;
}

Json ok_result() {
  Json::Object result;
  result["ok"] = Json::boolean_value(true);
  return Json::object_value(std::move(result));
}

void require_ok(mol_result_t result, const char* operation) {
  if (result != MOL_OK)
    throw RpcError(kRuntimeError, std::string(operation) + ": " + mol_result_string(result));
}

mol_command_t command(mol_command_type_t type) {
  mol_command_t result{};
  result.struct_size = static_cast<std::uint32_t>(sizeof(result));
  result.api_version = MOL_API_VERSION;
  result.command_type = type;
  result.source_id = kServiceSourceId;
  result.target_frame = MOL_FRAME_IMMEDIATE;
  return result;
}

Json state_json(const mol_engine_state_t& state) {
  Json::Object result;
  result["active_gestures"] = Json::number(state.active_gestures);
  result["active_voices"] = Json::number(state.active_voices);
  result["arpeggiator_gate"] = Json::number(state.arpeggiator_gate);
  result["arpeggiator_mode"] = Json::number(state.arpeggiator_mode);
  result["arpeggiator_octaves"] = Json::number(state.arpeggiator_octaves);
  result["arpeggiator_rate"] = Json::number(state.arpeggiator_rate);
  result["channel_count"] = Json::number(state.channel_count);
  result["chord_mode"] = Json::number(state.chord_mode);
  result["current_frame"] = Json::number(state.current_frame);
  result["loaded_sequence_event_count"] = Json::number(state.loaded_sequence_event_count);
  result["max_voices"] = Json::number(state.max_voices);
  result["metronome_enabled"] = Json::boolean_value(state.metronome_enabled != 0u);
  result["octave_shift"] = Json::number(static_cast<std::int64_t>(state.octave_shift));
  result["pitch_bend"] = Json::number(state.pitch_bend);
  result["playback"] = Json::boolean_value(state.playback != 0u);
  result["portamento_mode"] = Json::number(state.portamento_mode);
  result["portamento_time_ms"] = Json::number(state.portamento_time_ms);
  result["preset"] = Json::number(state.preset);
  result["recording"] = Json::boolean_value(state.recording != 0u);
  result["recording_event_count"] = Json::number(state.recording_event_count);
  result["sample_rate"] = Json::number(state.sample_rate);
  result["scale_mapping"] = Json::number(state.scale_mapping);
  result["scale_tonic"] = Json::number(state.scale_tonic);
  result["scale_type"] = Json::number(state.scale_type);
  result["sustain"] = Json::number(state.sustain);
  result["tempo"] = Json::number(state.tempo);
  result["time_signature_denominator"] = Json::number(state.time_signature_denominator);
  result["time_signature_numerator"] = Json::number(state.time_signature_numerator);
  result["transpose"] = Json::number(static_cast<std::int64_t>(state.transpose));
  result["transport_frame"] = Json::number(state.transport_frame);
  result["transport_running"] = Json::boolean_value(state.transport_running != 0u);
  return Json::object_value(std::move(result));
}

Json device_json(const DeviceInfo& device) {
  Json::Object result;
  result["active"] = Json::boolean_value(device.is_active);
  result["backend"] = Json::string(device.backend);
  result["bluetooth"] = Json::boolean_value(device.is_bluetooth);
  result["default"] = Json::boolean_value(device.is_default);
  result["id"] = Json::string(device.id);
  result["name"] = Json::string(device.name);
  result["physical_input"] = Json::boolean_value(device.is_physical_input);
  return Json::object_value(std::move(result));
}

Json devices_json(const std::vector<DeviceInfo>& devices) {
  Json::Array result;
  result.reserve(devices.size());
  for (const DeviceInfo& device : devices) result.push_back(device_json(device));
  return Json::array_value(std::move(result));
}

Json audio_status_json(const AudioStatus& status) {
  Json::Object result;
  result["available"] = Json::boolean_value(status.available);
  result["backend"] = Json::string(status.backend);
  result["channel_count"] = Json::number(status.channel_count);
  result["device_id"] = Json::string(status.device_id);
  result["device_name"] = Json::string(status.device_name);
  result["estimated_latency_ms"] = Json::number(status.estimated_latency_ms);
  result["low_latency_requested"] = Json::boolean_value(status.low_latency_requested);
  result["null_sink"] = Json::boolean_value(status.null_sink);
  result["period_frames"] = Json::number(status.period_frames);
  result["periods"] = Json::number(status.periods);
  result["sample_rate"] = Json::number(status.sample_rate);
  return Json::object_value(std::move(result));
}

Json patch_json(mol_preset_id_t preset) {
  mol_patch_t patch{};
  patch.struct_size = static_cast<std::uint32_t>(sizeof(patch));
  require_ok(mol_builtin_patch_load(preset, &patch), "load preset");
  Json::Object result;
  result["attack_ms"] = Json::number(patch.attack_ms);
  result["chorus_send_milli"] = Json::number(patch.chorus_send_milli);
  result["decay_ms"] = Json::number(patch.decay_ms);
  result["delay_send_milli"] = Json::number(patch.delay_send_milli);
  result["filter_cutoff_hz"] = Json::number(patch.filter_cutoff_hz);
  result["filter_resonance_milli"] = Json::number(patch.filter_resonance_milli);
  result["gain_millidb"] = Json::number(patch.gain_millidb);
  result["preset"] = Json::number(preset);
  result["release_ms"] = Json::number(patch.release_ms);
  result["reverb_send_milli"] = Json::number(patch.reverb_send_milli);
  result["stable_id"] = Json::string(mol_preset_stable_id(preset));
  result["sustain_milli"] = Json::number(patch.sustain_milli);
  result["synthesis_model"] = Json::number(patch.synthesis_model);
  result["waveform"] = Json::number(patch.waveform);
  return Json::object_value(std::move(result));
}

mol_preset_id_t parse_preset(const Json& value) {
  if (value.type == Json::Type::Number)
    return static_cast<mol_preset_id_t>(u64_value(value, "preset", MOL_PRESET_COUNT - 1u));
  const std::string id = string_value(value, "preset", 64u);
  for (mol_preset_id_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset)
    if (id == mol_preset_stable_id(preset)) return preset;
  throw RpcError(kInvalidParams, "unknown preset: " + id);
}

std::filesystem::path safe_recording_path(const std::filesystem::path& directory,
                                          const std::string& name) {
  constexpr std::string_view suffix = ".molseq";
  bool contains_control = false;
  for (const unsigned char character : name)
    if (character < 0x20u) contains_control = true;
  if (name.size() <= suffix.size() || name.size() > 128u ||
      name.find_first_of("/\\:") != std::string::npos || contains_control ||
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
    throw RpcError(kInvalidParams, "recording name must be a .molseq file name");
  return directory / std::filesystem::path(name);
}

Json make_default_config() {
  Json::Object mapping;
  Json::Object ipc;
  ipc["transport"] = Json::string("platform-local");
  Json::Object config;
  config["bpm"] = Json::number(100);
  config["chord_mode"] = Json::number(0);
  config["default_preset"] = Json::string("grand-piano");
  config["esp32_board_config"] = Json::string("none");
  config["input_device_id"] = Json::string("programmatic");
  config["input_mapping"] = Json::object_value(std::move(mapping));
  config["ipc"] = Json::object_value(std::move(ipc));
  config["log_level"] = Json::string("info");
  config["master_gain"] = Json::number(0.25);
  config["max_voices"] = Json::number(32);
  config["output_device_id"] = Json::string("default");
  config["sample_rate_policy"] = Json::string("device");
  config["scale_mapping"] = Json::number(0);
  config["scale_tonic"] = Json::number(0);
  config["scale_type"] = Json::number(0);
  config["schema_version"] = Json::number(1);
  config["web_ui"] = Json::boolean_value(false);
  return Json::object_value(std::move(config));
}

bool directory_is_writable(const std::filesystem::path& directory) {
  const std::filesystem::path probe =
      directory / (".doctor-write-" +
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::ofstream stream(probe, std::ios::binary | std::ios::trunc);
  const char marker = 'M';
  const bool written = stream && stream.write(&marker, 1);
  stream.close();
  std::error_code ignored;
  std::filesystem::remove(probe, ignored);
  return written && !ignored;
}

bool builtin_patches_are_valid() {
  for (mol_preset_id_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset) {
    mol_patch_t patch{};
    patch.struct_size = static_cast<std::uint32_t>(sizeof(patch));
    if (mol_builtin_patch_load(preset, &patch) != MOL_OK || mol_patch_validate(&patch) != MOL_OK)
      return false;
  }
  return true;
}

void validate_config(const Json& config) {
  require_object(config);
  static const std::set<std::string> expected = {"bpm",
                                                 "chord_mode",
                                                 "default_preset",
                                                 "esp32_board_config",
                                                 "input_device_id",
                                                 "input_mapping",
                                                 "ipc",
                                                 "log_level",
                                                 "master_gain",
                                                 "max_voices",
                                                 "output_device_id",
                                                 "sample_rate_policy",
                                                 "scale_mapping",
                                                 "scale_tonic",
                                                 "scale_type",
                                                 "schema_version",
                                                 "web_ui"};
  if (config.object.size() != expected.size())
    throw RpcError(kInvalidParams, "configuration fields do not match schema version 1");
  for (const auto& member : config.object)
    if (expected.find(member.first) == expected.end())
      throw RpcError(kInvalidParams, "unknown configuration field: " + member.first);
  if (u64_value(required(config, "schema_version"), "schema_version", 1u) != 1u)
    throw RpcError(kInvalidParams, "schema_version must be 1");
  (void)parse_preset(
      Json::string(string_value(required(config, "default_preset"), "default_preset", 64u)));
  (void)real_value(required(config, "master_gain"), "master_gain", 0.0, 2.0);
  (void)real_value(required(config, "bpm"), "bpm", MOL_TEMPO_MIN, MOL_TEMPO_MAX);
  const std::uint64_t max_voices = u64_value(required(config, "max_voices"), "max_voices", 64u);
  if (max_voices < 8u) throw RpcError(kInvalidParams, "max_voices must be 8..64");
  (void)u64_value(required(config, "scale_type"), "scale_type", MOL_SCALE_TYPE_COUNT - 1u);
  (void)u64_value(required(config, "scale_tonic"), "scale_tonic", 11u);
  (void)u64_value(required(config, "scale_mapping"), "scale_mapping", MOL_SCALE_MAPPING_COUNT - 1u);
  (void)u64_value(required(config, "chord_mode"), "chord_mode", MOL_CHORD_MODE_COUNT - 1u);
  const std::string sample_rate =
      string_value(required(config, "sample_rate_policy"), "sample_rate_policy", 16u);
  if (sample_rate != "device" && sample_rate != "44100" && sample_rate != "48000" &&
      sample_rate != "96000")
    throw RpcError(kInvalidParams, "sample_rate_policy must be device, 44100, 48000, or 96000");
  (void)string_value(required(config, "input_device_id"), "input_device_id", 512u);
  (void)string_value(required(config, "output_device_id"), "output_device_id", 512u);
  (void)string_value(required(config, "esp32_board_config"), "esp32_board_config", 256u);
  const std::string log_level = string_value(required(config, "log_level"), "log_level", 16u);
  if (log_level != "error" && log_level != "warning" && log_level != "info" && log_level != "debug")
    throw RpcError(kInvalidParams, "log_level must be error, warning, info, or debug");
  (void)bool_value(required(config, "web_ui"), "web_ui");
  const Json& mapping = required(config, "input_mapping");
  if (mapping.type != Json::Type::Object || mapping.object.size() > 128u)
    throw RpcError(kInvalidParams, "input_mapping must contain at most 128 entries");
  for (const auto& entry : mapping.object) {
    if (entry.first.empty() || entry.first.size() > 32u)
      throw RpcError(kInvalidParams, "input_mapping contains an invalid key");
    (void)u64_value(entry.second, "input_mapping note", 127u);
  }
  const Json& ipc = required(config, "ipc");
  allow_members(ipc, {"transport"});
  if (string_value(required(ipc, "transport"), "ipc.transport", 32u) != "platform-local")
    throw RpcError(kInvalidParams, "ipc.transport must be platform-local");
}

Json load_config_file(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error)) return make_default_config();
  if (error || !std::filesystem::is_regular_file(path, error))
    throw std::runtime_error("configuration path is not a regular file");
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error || size > kMaximumConfigBytes)
    throw std::runtime_error("configuration exceeds the 64 KiB limit");
  std::ifstream stream(path, std::ios::binary);
  std::string source(static_cast<std::size_t>(size), '\0');
  if (!stream ||
      (!source.empty() && !stream.read(source.data(), static_cast<std::streamsize>(size))))
    throw std::runtime_error("cannot read configuration file");
  return parse_service_config(source);
}

void save_config_file(const std::filesystem::path& path, const Json& config) {
  validate_config(config);
  const std::string text = molseq::write_json(config) + "\n";
  const std::filesystem::path temporary = path.string() + ".tmp";
  std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
  if (!stream || !stream.write(text.data(), static_cast<std::streamsize>(text.size())))
    throw std::runtime_error("cannot write temporary configuration file");
  stream.close();
  if (!stream) throw std::runtime_error("cannot finalize temporary configuration file");
#if defined(_WIN32)
  if (MoveFileExW(temporary.c_str(), path.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
    const unsigned long windows_error = static_cast<unsigned long>(GetLastError());
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error("cannot replace configuration file; Windows error " +
                             std::to_string(windows_error));
  }
#else
  if (::chmod(temporary.c_str(), S_IRUSR | S_IWUSR) != 0 ||
      std::rename(temporary.c_str(), path.c_str()) != 0) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error("cannot securely replace configuration file");
  }
#endif
}

Json single_config_value(const Json& config, const Json& params) {
  const Json* key_value = molseq::optional_member(params, "key");
  if (key_value == nullptr) return config;
  const std::string key = string_value(*key_value, "key", 64u);
  const Json* value = molseq::optional_member(config, key);
  if (value == nullptr) throw RpcError(kInvalidParams, "unknown configuration key: " + key);
  Json::Object result;
  result[key] = *value;
  return Json::object_value(std::move(result));
}

}  // namespace

molseq::Json parse_service_config(std::string_view source) {
  if (source.size() > kMaximumConfigBytes)
    throw std::runtime_error("configuration exceeds the 64 KiB limit");
  Json config = molseq::parse_json(std::string(source));
  validate_config(config);
  return config;
}

ServiceBackend::ServiceBackend(ServiceRuntime& runtime,
                               const std::filesystem::path& state_directory)
    : config_(make_default_config()),
      runtime_(runtime),
      state_directory_(std::filesystem::absolute(state_directory).lexically_normal()),
      recordings_directory_(state_directory_ / "recordings"),
      config_path_(state_directory_ / "config.json") {
  std::error_code error;
  std::filesystem::create_directories(recordings_directory_, error);
  if (error) throw std::runtime_error("cannot create service state directory: " + error.message());
  const bool had_config = std::filesystem::exists(config_path_, error);
  if (error) throw std::runtime_error("cannot inspect service configuration: " + error.message());
  config_ = load_config_file(config_path_);
  if (!had_config) persist_config();

  const std::string output_id =
      string_value(required(config_, "output_device_id"), "output_device_id", 512u);
  if (output_id != runtime_.audio_status().device_id) (void)runtime_.select_output(output_id);

  mol_command_t preset = command(MOL_COMMAND_SET_PRESET);
  preset.payload.preset.preset = parse_preset(required(config_, "default_preset"));
  require_ok(runtime_.submit(preset), "apply configured preset");
  mol_command_t gain = command(MOL_COMMAND_SET_MASTER_GAIN);
  gain.payload.scalar.value =
      static_cast<float>(real_value(required(config_, "master_gain"), "master_gain", 0.0, 2.0));
  require_ok(runtime_.submit(gain), "apply configured master gain");
  mol_command_t tempo = command(MOL_COMMAND_SET_TEMPO);
  tempo.payload.scalar.value =
      static_cast<float>(real_value(required(config_, "bpm"), "bpm", MOL_TEMPO_MIN, MOL_TEMPO_MAX));
  require_ok(runtime_.submit(tempo), "apply configured tempo");
  mol_command_t scale = command(MOL_COMMAND_SET_SCALE);
  scale.payload.scale.type = static_cast<std::uint32_t>(
      u64_value(required(config_, "scale_type"), "scale_type", MOL_SCALE_TYPE_COUNT - 1u));
  scale.payload.scale.tonic =
      static_cast<std::uint8_t>(u64_value(required(config_, "scale_tonic"), "scale_tonic", 11u));
  scale.payload.scale.mapping = static_cast<std::uint8_t>(
      u64_value(required(config_, "scale_mapping"), "scale_mapping", MOL_SCALE_MAPPING_COUNT - 1u));
  require_ok(runtime_.submit(scale), "apply configured scale");
  mol_command_t chord = command(MOL_COMMAND_SET_CHORD_MODE);
  chord.payload.integer.value = static_cast<std::int32_t>(
      u64_value(required(config_, "chord_mode"), "chord_mode", MOL_CHORD_MODE_COUNT - 1u));
  require_ok(runtime_.submit(chord), "apply configured chord");
  const std::string input_id =
      string_value(required(config_, "input_device_id"), "input_device_id", 512u);
  (void)runtime_.attach_input(input_id);
}

void ServiceBackend::persist_config() const { save_config_file(config_path_, config_); }

Json ServiceBackend::invoke(std::string_view method, const Json& params) {
  return invoke_checked(method, params);
}

Json ServiceBackend::invoke_checked(std::string_view method, const Json& params) {
  require_object(params);
  if (method == "system.getInfo") {
    allow_members(params, {});
    Json::Object result;
    result["api_version"] = Json::number(MOL_API_VERSION);
    result["name"] = Json::string("MoL Keyboard Service");
    result["offline"] = Json::boolean_value(true);
    result["version"] = Json::string(mol_get_version_string());
    return Json::object_value(std::move(result));
  }
  if (method == "system.getCapabilities") {
    allow_members(params, {});
    Json::Object effects;
    const mol_capability_flags_t flags = runtime_.capabilities();
    effects["chorus"] = Json::boolean_value((flags & MOL_CAPABILITY_CHORUS) != 0u);
    effects["delay"] = Json::boolean_value((flags & MOL_CAPABILITY_DELAY) != 0u);
    effects["limiter"] = Json::boolean_value((flags & MOL_CAPABILITY_LIMITER) != 0u);
    effects["reverb"] = Json::boolean_value((flags & MOL_CAPABILITY_REVERB) != 0u);
    Json::Array channels{Json::number(1), Json::number(2)};
    Json::Array rates{Json::number(8000), Json::number(44100), Json::number(48000),
                      Json::number(96000), Json::number(192000)};
    mol_engine_state_t state{};
    state.struct_size = static_cast<std::uint32_t>(sizeof(state));
    require_ok(runtime_.snapshot(state), "get capabilities state");
    Json::Object result;
    result["a2dp_source"] = Json::boolean_value(false);
    result["background_audio"] = Json::string("user-service");
    result["build_profile"] = Json::string("standard");
    result["channel_counts"] = Json::array_value(std::move(channels));
    result["effects"] = Json::object_value(std::move(effects));
    result["gpio"] = Json::boolean_value(false);
    const std::vector<DeviceInfo> inputs = runtime_.input_devices();
    const bool physical_input =
        std::any_of(inputs.begin(), inputs.end(),
                    [](const DeviceInfo& item) { return item.is_physical_input; });
    result["hid"] = Json::boolean_value(physical_input);
    result["max_voices"] = Json::number(state.max_voices);
    result["midi"] = Json::boolean_value(false);
    result["persistent_storage"] = Json::boolean_value(true);
    result["sample_rates"] = Json::array_value(std::move(rates));
    result["sampler"] = Json::boolean_value(false);
    result["ui"] = Json::boolean_value(false);
    result["web_shared_array_buffer"] = Json::boolean_value(false);
    return Json::object_value(std::move(result));
  }
  if (method == "system.getMetrics") {
    allow_members(params, {});
    const RuntimeMetrics metrics = runtime_.metrics();
    Json::Object result;
    result["callbacks"] = Json::number(metrics.callbacks);
    result["device_notifications"] = Json::number(metrics.device_notifications);
    result["device_reroutes"] = Json::number(metrics.device_reroutes);
    result["dropped_commands"] = Json::number(metrics.dropped_commands);
    result["dropped_events"] = Json::number(metrics.dropped_events);
    result["input_events"] = Json::number(metrics.input_events);
    result["non_finite_samples"] = Json::number(metrics.non_finite_samples);
    result["render_failures"] = Json::number(metrics.render_failures);
    result["rendered_frames"] = Json::number(metrics.rendered_frames);
    result["underruns"] = Json::number(metrics.underruns);
    return Json::object_value(std::move(result));
  }
  if (method == "system.shutdown") {
    allow_members(params, {});
    runtime_.request_shutdown();
    return ok_result();
  }
  if (method == "engine.getState") {
    allow_members(params, {});
    mol_engine_state_t state{};
    state.struct_size = static_cast<std::uint32_t>(sizeof(state));
    require_ok(runtime_.snapshot(state), "get engine state");
    return state_json(state);
  }
  if (method == "engine.reset" || method == "engine.allNotesOff" ||
      method == "engine.allSoundOff") {
    allow_members(params, {});
    mol_command_type_t type = MOL_COMMAND_RESET_ENGINE;
    if (method == "engine.allNotesOff") type = MOL_COMMAND_ALL_NOTES_OFF;
    if (method == "engine.allSoundOff") type = MOL_COMMAND_ALL_SOUND_OFF;
    require_ok(runtime_.submit(command(type)), "submit engine command");
    return ok_result();
  }
  if (method == "preset.list") {
    allow_members(params, {});
    Json::Array result;
    for (mol_preset_id_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset) {
      Json::Object item;
      item["english_name"] = Json::string(mol_preset_english_name(preset));
      item["id"] = Json::number(preset);
      item["name"] = Json::string(mol_preset_stable_id(preset));
      item["zh_name"] = Json::string(mol_preset_chinese_name(preset));
      result.push_back(Json::object_value(std::move(item)));
    }
    return Json::array_value(std::move(result));
  }
  if (method == "preset.select") {
    allow_members(params, {"preset", "hard"});
    mol_command_t value = command(MOL_COMMAND_SET_PRESET);
    value.payload.preset.preset = parse_preset(required(params, "preset"));
    const Json* hard = molseq::optional_member(params, "hard");
    value.payload.preset.hard_switch =
        hard != nullptr && bool_value(*hard, "hard") ? static_cast<std::uint8_t>(1u) : 0u;
    require_ok(runtime_.submit(value), "select preset");
    config_.object["default_preset"] =
        Json::string(mol_preset_stable_id(value.payload.preset.preset));
    persist_config();
    return ok_result();
  }
  if (method == "preset.getParameters") {
    allow_members(params, {"preset"});
    const Json* preset = molseq::optional_member(params, "preset");
    if (preset != nullptr) return patch_json(parse_preset(*preset));
    mol_engine_state_t state{};
    state.struct_size = static_cast<std::uint32_t>(sizeof(state));
    require_ok(runtime_.snapshot(state), "get preset state");
    return patch_json(state.preset);
  }
  if (method == "preset.setParameter") {
    allow_members(params, {"parameter", "value"});
    mol_command_t value = command(MOL_COMMAND_SET_PARAMETER);
    value.payload.parameter.parameter = static_cast<std::uint32_t>(
        u64_value(required(params, "parameter"), "parameter", MOL_PARAMETER_LIMITER_CEILING_DB));
    value.payload.parameter.value =
        static_cast<float>(real_value(required(params, "value"), "value", -100000.0, 100000.0));
    require_ok(runtime_.submit(value), "set preset parameter");
    return ok_result();
  }
  if (method == "transport.get") {
    allow_members(params, {});
    mol_engine_state_t state{};
    state.struct_size = static_cast<std::uint32_t>(sizeof(state));
    require_ok(runtime_.snapshot(state), "get transport state");
    Json::Object result;
    result["denominator"] = Json::number(state.time_signature_denominator);
    result["frame"] = Json::number(state.transport_frame);
    result["numerator"] = Json::number(state.time_signature_numerator);
    result["running"] = Json::boolean_value(state.transport_running != 0u);
    result["tempo"] = Json::number(state.tempo);
    return Json::object_value(std::move(result));
  }
  if (method == "transport.setTempo") {
    allow_members(params, {"bpm"});
    mol_command_t value = command(MOL_COMMAND_SET_TEMPO);
    value.payload.scalar.value = static_cast<float>(
        real_value(required(params, "bpm"), "bpm", MOL_TEMPO_MIN, MOL_TEMPO_MAX));
    require_ok(runtime_.submit(value), "set tempo");
    config_.object["bpm"] = Json::number(value.payload.scalar.value);
    persist_config();
    return ok_result();
  }
  if (method == "transport.setTimeSignature") {
    allow_members(params, {"numerator", "denominator"});
    mol_command_t value = command(MOL_COMMAND_SET_TIME_SIGNATURE);
    value.payload.time_signature.numerator =
        static_cast<std::uint8_t>(u64_value(required(params, "numerator"), "numerator", 255u));
    value.payload.time_signature.denominator =
        static_cast<std::uint8_t>(u64_value(required(params, "denominator"), "denominator", 255u));
    require_ok(runtime_.submit(value), "set time signature");
    return ok_result();
  }
  if (method == "transport.start" || method == "transport.stop") {
    allow_members(params, {});
    require_ok(runtime_.submit(command(method == "transport.start" ? MOL_COMMAND_TRANSPORT_START
                                                                   : MOL_COMMAND_TRANSPORT_STOP)),
               "set transport state");
    return ok_result();
  }
  if (method == "input.listDevices") {
    allow_members(params, {});
    return devices_json(runtime_.input_devices());
  }
  if (method == "input.attach") {
    allow_members(params, {"id"});
    const std::string id = string_value(required(params, "id"), "id", 512u);
    require_ok(runtime_.attach_input(id), "attach input");
    config_.object["input_device_id"] = Json::string(id);
    persist_config();
    return ok_result();
  }
  if (method == "input.detach") {
    allow_members(params, {});
    require_ok(runtime_.detach_input(), "detach input");
    config_.object["input_device_id"] = Json::string("programmatic");
    persist_config();
    return ok_result();
  }
  if (method == "input.getMapping") {
    allow_members(params, {});
    Json::Object result;
    result["active_input_id"] = Json::string(runtime_.active_input_id());
    result["mapping"] = *molseq::optional_member(config_, "input_mapping");
    return Json::object_value(std::move(result));
  }
  if (method == "input.setMapping") {
    allow_members(params, {"mapping"});
    const Json& mapping = required(params, "mapping");
    if (mapping.type != Json::Type::Object || mapping.object.size() > 128u)
      throw RpcError(kInvalidParams, "mapping must be an object with at most 128 entries");
    Json candidate = config_;
    candidate.object["input_mapping"] = mapping;
    validate_config(candidate);
    config_ = std::move(candidate);
    persist_config();
    return ok_result();
  }
  if (method == "audio.listDevices") {
    allow_members(params, {});
    return devices_json(runtime_.output_devices());
  }
  if (method == "audio.selectDevice") {
    allow_members(params, {"id"});
    const std::string id = string_value(required(params, "id"), "id", 512u);
    require_ok(runtime_.select_output(id), "select output");
    config_.object["output_device_id"] = Json::string(id);
    persist_config();
    return audio_status_json(runtime_.audio_status());
  }
  if (method == "audio.getLatency") {
    allow_members(params, {});
    return audio_status_json(runtime_.audio_status());
  }
  if (method == "performance.noteOn") {
    allow_members(params, {"note", "velocity", "gesture", "source"});
    mol_command_t value = command(MOL_COMMAND_NOTE_ON);
    value.payload.note.note =
        static_cast<std::uint8_t>(u64_value(required(params, "note"), "note", 127u));
    const Json* velocity = molseq::optional_member(params, "velocity");
    value.payload.note.velocity = static_cast<float>(
        velocity == nullptr ? 0.8 : real_value(*velocity, "velocity", 0.000001, 1.0));
    const Json* gesture = molseq::optional_member(params, "gesture");
    if (gesture == nullptr) {
      value.gesture_id = next_gesture_id_++;
      if (next_gesture_id_ > UINT64_C(9007199254740991))
        next_gesture_id_ = UINT64_C(0x535256000001);
    } else {
      value.gesture_id = u64_value(*gesture, "gesture", UINT64_MAX);
    }
    const Json* source = molseq::optional_member(params, "source");
    value.source_id = source == nullptr
                          ? kServiceSourceId
                          : static_cast<std::uint32_t>(u64_value(*source, "source", UINT32_MAX));
    require_ok(runtime_.submit(value), "note on");
    Json::Object result;
    result["gesture"] = Json::number(value.gesture_id);
    result["ok"] = Json::boolean_value(true);
    return Json::object_value(std::move(result));
  }
  if (method == "performance.noteOff") {
    allow_members(params, {"gesture", "source"});
    mol_command_t value = command(MOL_COMMAND_NOTE_OFF);
    value.gesture_id = u64_value(required(params, "gesture"), "gesture", UINT64_MAX);
    const Json* source = molseq::optional_member(params, "source");
    value.source_id = source == nullptr
                          ? kServiceSourceId
                          : static_cast<std::uint32_t>(u64_value(*source, "source", UINT32_MAX));
    require_ok(runtime_.submit(value), "note off");
    return ok_result();
  }
  if (method == "performance.control") {
    allow_members(params, {"control", "value", "mode", "rate", "gate", "octaves", "random_seed",
                           "enabled", "level", "time_ms", "type", "tonic", "mapping"});
    const std::string control = string_value(required(params, "control"), "control", 32u);
    mol_command_t value = command(MOL_COMMAND_SUSTAIN);
    if (control == "arpeggiator") {
      value.command_type = MOL_COMMAND_SET_ARPEGGIATOR;
      value.payload.arpeggiator.mode = static_cast<std::uint32_t>(
          u64_value(required(params, "mode"), "mode", MOL_ARPEGGIATOR_MODE_COUNT - 1u));
      value.payload.arpeggiator.rate = static_cast<std::uint32_t>(
          u64_value(required(params, "rate"), "rate", MOL_ARPEGGIATOR_RATE_COUNT - 1u));
      const Json* gate = molseq::optional_member(params, "gate");
      value.payload.arpeggiator.gate =
          static_cast<float>(gate == nullptr ? 0.5 : real_value(*gate, "gate", 0.05, 1.0));
      const Json* octaves = molseq::optional_member(params, "octaves");
      value.payload.arpeggiator.octaves =
          static_cast<std::uint8_t>(octaves == nullptr ? 1u : u64_value(*octaves, "octaves", 4u));
      if (value.payload.arpeggiator.octaves == 0u)
        throw RpcError(kInvalidParams, "octaves must be 1..4");
      const Json* seed = molseq::optional_member(params, "random_seed");
      value.payload.arpeggiator.random_seed = static_cast<std::uint32_t>(
          seed == nullptr ? UINT32_C(0x4d4f4c31) : u64_value(*seed, "random_seed", UINT32_MAX));
      require_ok(runtime_.submit(value), "performance control");
      return ok_result();
    }
    if (control == "scale") {
      value.command_type = MOL_COMMAND_SET_SCALE;
      value.payload.scale.type = static_cast<std::uint32_t>(
          u64_value(required(params, "type"), "type", MOL_SCALE_TYPE_COUNT - 1u));
      value.payload.scale.tonic =
          static_cast<std::uint8_t>(u64_value(required(params, "tonic"), "tonic", 11u));
      value.payload.scale.mapping = static_cast<std::uint8_t>(
          u64_value(required(params, "mapping"), "mapping", MOL_SCALE_MAPPING_COUNT - 1u));
      require_ok(runtime_.submit(value), "performance control");
      return ok_result();
    }
    if (control == "metronome") {
      value.command_type = MOL_COMMAND_SET_METRONOME;
      value.payload.metronome.enabled =
          bool_value(required(params, "enabled"), "enabled") ? 1u : 0u;
      value.payload.metronome.level =
          static_cast<float>(real_value(required(params, "level"), "level", 0.0, 1.0));
      require_ok(runtime_.submit(value), "performance control");
      return ok_result();
    }
    if (control == "portamento") {
      value.command_type = MOL_COMMAND_SET_PORTAMENTO;
      value.payload.portamento.mode = static_cast<std::uint32_t>(
          u64_value(required(params, "mode"), "mode", MOL_PORTAMENTO_MODE_COUNT - 1u));
      value.payload.portamento.time_ms =
          static_cast<float>(real_value(required(params, "time_ms"), "time_ms", 0.0, 2000.0));
      require_ok(runtime_.submit(value), "performance control");
      return ok_result();
    }
    const double scalar = real_value(required(params, "value"), "value", -24.0, 24.0);
    if (control == "sustain") {
      if (scalar < 0.0 || scalar > 1.0) throw RpcError(kInvalidParams, "sustain must be 0..1");
      value.payload.scalar.value = static_cast<float>(scalar);
    } else if (control == "pitch-bend") {
      if (scalar < -1.0 || scalar > 1.0) throw RpcError(kInvalidParams, "pitch-bend must be -1..1");
      value.command_type = MOL_COMMAND_PITCH_BEND;
      value.payload.scalar.value = static_cast<float>(scalar);
    } else if (control == "master-gain") {
      if (scalar < 0.0 || scalar > 2.0) throw RpcError(kInvalidParams, "master-gain must be 0..2");
      value.command_type = MOL_COMMAND_SET_MASTER_GAIN;
      value.payload.scalar.value = static_cast<float>(scalar);
    } else if (control == "chord") {
      value.command_type = MOL_COMMAND_SET_CHORD_MODE;
      value.payload.integer.value = static_cast<std::int32_t>(scalar);
    } else if (control == "octave") {
      if (scalar < -3.0 || scalar > 3.0 || scalar != std::trunc(scalar))
        throw RpcError(kInvalidParams, "octave must be an integer from -3..3");
      value.command_type = MOL_COMMAND_SET_OCTAVE_SHIFT;
      value.payload.integer.value = static_cast<std::int32_t>(scalar);
    } else if (control == "transpose") {
      if (scalar != std::trunc(scalar))
        throw RpcError(kInvalidParams, "transpose must be an integer from -24..24");
      value.command_type = MOL_COMMAND_SET_TRANSPOSE;
      value.payload.integer.value = static_cast<std::int32_t>(scalar);
    } else {
      throw RpcError(kInvalidParams, "unknown performance control: " + control);
    }
    require_ok(runtime_.submit(value), "performance control");
    if (control == "chord") {
      config_.object["chord_mode"] = Json::number(value.payload.integer.value);
      persist_config();
    }
    return ok_result();
  }
  if (method == "recording.start" || method == "recording.stop") {
    allow_members(params, method == "recording.stop"
                              ? std::initializer_list<std::string_view>{"name"}
                              : std::initializer_list<std::string_view>{});
    require_ok(runtime_.submit(command(method == "recording.start" ? MOL_COMMAND_RECORD_START
                                                                   : MOL_COMMAND_RECORD_STOP)),
               "set recording state");
    const Json* name = molseq::optional_member(params, "name");
    if (name != nullptr) {
      molseq::SequenceDocument document;
      require_ok(runtime_.copy_recording(document), "copy recording");
      const std::string file_name = string_value(*name, "name", 128u);
      molseq::save_binary(safe_recording_path(recordings_directory_, file_name).string(), document);
    }
    return ok_result();
  }
  if (method == "recording.list") {
    allow_members(params, {});
    Json::Array result;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(recordings_directory_, error), end;
         !error && iterator != end && result.size() < 1024u; iterator.increment(error)) {
      if (iterator->is_regular_file(error) && iterator->path().extension() == ".molseq")
        result.push_back(Json::string(iterator->path().filename().string()));
    }
    if (error) throw RpcError(kRuntimeError, "cannot list recordings: " + error.message());
    std::sort(result.begin(), result.end(),
              [](const Json& left, const Json& right) { return left.text < right.text; });
    return Json::array_value(std::move(result));
  }
  if (method == "recording.load") {
    allow_members(params, {"name"});
    const std::string name = string_value(required(params, "name"), "name", 128u);
    const molseq::SequenceDocument document =
        molseq::load_binary(safe_recording_path(recordings_directory_, name).string());
    require_ok(runtime_.load_sequence(document), "load recording");
    return ok_result();
  }
  if (method == "recording.save") {
    allow_members(params, {"name"});
    const std::string name = string_value(required(params, "name"), "name", 128u);
    molseq::SequenceDocument document;
    require_ok(runtime_.copy_recording(document), "copy recording");
    molseq::save_binary(safe_recording_path(recordings_directory_, name).string(), document);
    return ok_result();
  }
  if (method == "playback.start" || method == "playback.stop") {
    allow_members(params, method == "playback.start"
                              ? std::initializer_list<std::string_view>{"name"}
                              : std::initializer_list<std::string_view>{});
    const Json* name = molseq::optional_member(params, "name");
    if (name != nullptr) {
      const std::string file_name = string_value(*name, "name", 128u);
      const molseq::SequenceDocument document =
          molseq::load_binary(safe_recording_path(recordings_directory_, file_name).string());
      require_ok(runtime_.load_sequence(document), "load playback sequence");
    }
    require_ok(runtime_.submit(command(method == "playback.start" ? MOL_COMMAND_PLAYBACK_START
                                                                  : MOL_COMMAND_PLAYBACK_STOP)),
               "set playback state");
    return ok_result();
  }
  if (method == "playback.seek") {
    allow_members(params, {"frame"});
    mol_command_t value = command(MOL_COMMAND_TRANSPORT_SEEK);
    value.payload.transport.frame = u64_value(required(params, "frame"), "frame", UINT64_MAX - 1u);
    require_ok(runtime_.submit(value), "seek playback");
    return ok_result();
  }
  if (method == "config.get") {
    allow_members(params, {"key"});
    return single_config_value(config_, params);
  }
  if (method == "config.set") {
    allow_members(params, {"key", "value"});
    const std::string key = string_value(required(params, "key"), "key", 64u);
    const Json& value = required(params, "value");
    if (key == "schema_version" || config_.object.find(key) == config_.object.end())
      throw RpcError(kInvalidParams, "configuration key is read-only or unknown: " + key);
    Json candidate = config_;
    candidate.object[key] = value;
    validate_config(candidate);
    if (key == "bpm") {
      mol_command_t update = command(MOL_COMMAND_SET_TEMPO);
      update.payload.scalar.value =
          static_cast<float>(real_value(value, "value", MOL_TEMPO_MIN, MOL_TEMPO_MAX));
      require_ok(runtime_.submit(update), "set configured tempo");
    } else if (key == "master_gain") {
      mol_command_t update = command(MOL_COMMAND_SET_MASTER_GAIN);
      update.payload.scalar.value = static_cast<float>(real_value(value, "value", 0.0, 2.0));
      require_ok(runtime_.submit(update), "set configured master gain");
    } else if (key == "default_preset") {
      mol_command_t update = command(MOL_COMMAND_SET_PRESET);
      update.payload.preset.preset = parse_preset(value);
      require_ok(runtime_.submit(update), "set configured preset");
    } else if (key == "output_device_id") {
      require_ok(runtime_.select_output(string_value(value, "value", 512u)),
                 "set configured output");
    } else if (key == "input_device_id") {
      require_ok(runtime_.attach_input(string_value(value, "value", 512u)), "set configured input");
    } else if (key == "scale_type" || key == "scale_tonic" || key == "scale_mapping") {
      mol_command_t update = command(MOL_COMMAND_SET_SCALE);
      update.payload.scale.type = static_cast<std::uint32_t>(
          u64_value(required(candidate, "scale_type"), "scale_type", MOL_SCALE_TYPE_COUNT - 1u));
      update.payload.scale.tonic = static_cast<std::uint8_t>(
          u64_value(required(candidate, "scale_tonic"), "scale_tonic", 11u));
      update.payload.scale.mapping = static_cast<std::uint8_t>(u64_value(
          required(candidate, "scale_mapping"), "scale_mapping", MOL_SCALE_MAPPING_COUNT - 1u));
      require_ok(runtime_.submit(update), "set configured scale");
    } else if (key == "chord_mode") {
      mol_command_t update = command(MOL_COMMAND_SET_CHORD_MODE);
      update.payload.integer.value =
          static_cast<std::int32_t>(u64_value(value, "value", MOL_CHORD_MODE_COUNT - 1u));
      require_ok(runtime_.submit(update), "set configured chord");
    }
    config_ = std::move(candidate);
    persist_config();
    Json::Object result;
    result["ok"] = Json::boolean_value(true);
    result["restart_required"] =
        Json::boolean_value(key == "sample_rate_policy" || key == "max_voices" || key == "ipc");
    return Json::object_value(std::move(result));
  }
  if (method == "diagnostics.selfTest") {
    allow_members(params, {});
    const bool patches_ok = builtin_patches_are_valid();
    std::string detail;
    const bool runtime_ok = runtime_.runtime_self_test(detail);
    Json::Object result;
    result["ok"] = Json::boolean_value(patches_ok && runtime_ok);
    result["patches"] = Json::boolean_value(patches_ok);
    result["runtime"] = Json::boolean_value(runtime_ok);
    result["runtime_detail"] = Json::string(detail);
    return Json::object_value(std::move(result));
  }
  if (method == "diagnostics.doctor") {
    allow_members(params, {});
    Json::Array checks;
    const AudioStatus audio = runtime_.audio_status();
    const RuntimeMetrics metrics = runtime_.metrics();
    const bool patches_ok = builtin_patches_are_valid();
    const bool storage_writable = directory_is_writable(recordings_directory_);
    auto add_check = [&checks](const char* id, bool ok, const std::string& message,
                               const std::string& action) {
      Json::Object check;
      check["action"] = Json::string(action);
      check["id"] = Json::string(id);
      check["message"] = Json::string(message);
      check["status"] = Json::string(ok ? "pass" : "warning");
      checks.push_back(Json::object_value(std::move(check)));
    };
    add_check("core-abi", mol_get_api_version() == MOL_API_VERSION,
              "Core API and service ABI are compatible.", "Reinstall matching binaries.");
    add_check("audio-device", audio.available,
              audio.available ? "Audio output is initialized as " + audio.backend + "/" +
                                    audio.device_name + " at " + std::to_string(audio.sample_rate) +
                                    " Hz, " + std::to_string(audio.channel_count) + " channels."
                              : "No hardware output is active.",
              "Select a system output or start with --null-backend.");
    const bool latency_ok = audio.low_latency_requested && audio.estimated_latency_ms > 0.0 &&
                            audio.estimated_latency_ms <= 20.0;
    add_check("low-latency", latency_ok,
              "Estimated device buffering latency is " +
                  std::to_string(audio.estimated_latency_ms) + " ms; requested period is " +
                  std::to_string(audio.period_frames) + " frames x " +
                  std::to_string(audio.periods) + ".",
              "Use a supported native audio backend and reduce the device period.");
    const std::vector<DeviceInfo> outputs = runtime_.output_devices();
    const bool bluetooth = std::any_of(outputs.begin(), outputs.end(),
                                       [](const DeviceInfo& item) { return item.is_bluetooth; });
    add_check("bluetooth-output", bluetooth,
              bluetooth ? "The operating system exposes a Bluetooth output."
                        : "No Bluetooth output is exposed by the operating system.",
              "Pair the speaker in system settings; the service does not implement desktop A2DP.");
    const std::vector<DeviceInfo> inputs = runtime_.input_devices();
    const bool physical_input =
        std::any_of(inputs.begin(), inputs.end(),
                    [](const DeviceInfo& item) { return item.is_physical_input; });
    add_check("hid-input", physical_input,
              physical_input ? "A physical keyboard adapter is available."
                             : "No accessible physical keyboard input found.",
              "Grant input-monitoring permission or select an accessible keyboard.");
    add_check("midi-input", false, "Desktop MIDI input is not enabled in this service build.",
              "Use the physical keyboard or local IPC input; install a MIDI-enabled build when "
              "available.");
    add_check("service-ipc", true, "This request reached the local service IPC.",
              "Restart the user service if future requests fail.");
    add_check("configuration", true, "Configuration schema version 1 was parsed and validated.",
              "Run config.get to inspect effective settings.");
    add_check("sound-pack", patches_ok,
              patches_ok ? "All 18 built-in patches decode and validate."
                         : "One or more built-in patches are invalid.",
              "Reinstall mol-keyboardd from a verified package.");
    add_check("storage", storage_writable,
              storage_writable ? "The private recordings directory passed a write probe."
                               : "The private recordings directory is not writable.",
              "Check ordinary-user write permission on the service state directory.");
    add_check("realtime-health", metrics.underruns == 0u && metrics.dropped_commands == 0u,
              "Realtime counters were inspected.",
              "Close CPU-heavy applications or increase the audio period if counters rise.");
    add_check("web-isolation", false, "The optional Web controller is not enabled in this build.",
              "Serve the Web build over HTTPS with COOP/COEP for SharedArrayBuffer.");
    add_check("esp32-capability", true, "Desktop build has no ESP32 chip capability claim.",
              "Run the firmware doctor on the target for chip-specific checks.");
#if defined(_WIN32)
    const std::string limitation =
        "Raw Input requires an interactive user session; Bluetooth audio is provided by WASAPI.";
#elif defined(__APPLE__)
    const std::string limitation =
        "IOHID input may require Input Monitoring permission; Bluetooth audio is provided by "
        "Core Audio.";
#else
    const std::string limitation =
        "evdev input requires read access to /dev/input/event*; Bluetooth audio is provided by "
        "the selected Linux audio server.";
#endif
    add_check("platform-limitations", true, limitation,
              "Review the desktop service platform guide before background deployment.");
    Json::Object result;
    result["checks"] = Json::array_value(std::move(checks));
    result["ok"] = Json::boolean_value(audio.available && patches_ok && storage_writable &&
                                       metrics.underruns == 0u && metrics.dropped_commands == 0u);
    return Json::object_value(std::move(result));
  }
  if (method == "diagnostics.benchmark") {
    allow_members(params, {"frames"});
    const Json* frame_value = molseq::optional_member(params, "frames");
    const std::uint64_t frames = frame_value == nullptr
                                     ? 96000u
                                     : u64_value(*frame_value, "frames", kMaximumBenchmarkFrames);
    if (frames == 0u) throw RpcError(kInvalidParams, "frames must be greater than zero");
    BenchmarkResult benchmark;
    require_ok(runtime_.benchmark(frames, benchmark), "run benchmark");
    Json::Object result;
    result["elapsed_ms"] = Json::number(benchmark.elapsed_ms);
    result["frames"] = Json::number(benchmark.frames);
    result["non_finite_samples"] = Json::number(benchmark.non_finite_samples);
    result["peak"] = Json::number(benchmark.peak);
    result["realtime_ratio"] = Json::number(benchmark.realtime_ratio);
    return Json::object_value(std::move(result));
  }
  throw RpcError(-32601, "Method not implemented");
}

}  // namespace molcontrol
