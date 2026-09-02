// SPDX-License-Identifier: Apache-2.0
#include "audio_runtime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "miniaudio.h"
#include "physical_input.hpp"

namespace molkeyboardd {
namespace {

constexpr std::size_t kEngineMemoryBytes = 1048576u;
constexpr std::size_t kCommandQueueCapacity = 1024u;
constexpr std::size_t kCommandQueueMask = kCommandQueueCapacity - 1u;
constexpr ma_uint32 kChannelCount = 2u;
constexpr ma_uint32 kRequestedPeriodFrames = 128u;
constexpr ma_uint32 kRequestedPeriods = 3u;
constexpr ma_uint32 kMaximumCommandsPerCallback = 256u;

static_assert((kCommandQueueCapacity & kCommandQueueMask) == 0u,
              "command queue capacity must be a power of two");

struct EngineMemory {
  alignas(std::max_align_t) std::array<unsigned char, kEngineMemoryBytes> bytes{};
};

class CommandQueue {
 public:
  CommandQueue() {
    for (std::size_t index = 0u; index < cells_.size(); ++index)
      cells_[index].sequence.store(index, std::memory_order_relaxed);
  }

  bool push(const mol_command_t& command) noexcept {
    Cell* cell = nullptr;
    std::size_t position = enqueue_position_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &cells_[position & kCommandQueueMask];
      const std::size_t sequence = cell->sequence.load(std::memory_order_acquire);
      const std::intptr_t difference =
          static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(position);
      if (difference == 0) {
        if (enqueue_position_.compare_exchange_weak(position, position + 1u,
                                                    std::memory_order_relaxed))
          break;
      } else if (difference < 0) {
        return false;
      } else {
        position = enqueue_position_.load(std::memory_order_relaxed);
      }
    }
    cell->command = command;
    cell->sequence.store(position + 1u, std::memory_order_release);
    return true;
  }

  bool pop(mol_command_t& command) noexcept {
    Cell* cell = nullptr;
    std::size_t position = dequeue_position_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &cells_[position & kCommandQueueMask];
      const std::size_t sequence = cell->sequence.load(std::memory_order_acquire);
      const std::intptr_t difference =
          static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(position + 1u);
      if (difference == 0) {
        if (dequeue_position_.compare_exchange_weak(position, position + 1u,
                                                    std::memory_order_relaxed))
          break;
      } else if (difference < 0) {
        return false;
      } else {
        position = dequeue_position_.load(std::memory_order_relaxed);
      }
    }
    command = cell->command;
    cell->sequence.store(position + kCommandQueueCapacity, std::memory_order_release);
    return true;
  }

 private:
  struct Cell {
    std::atomic<std::size_t> sequence{0u};
    mol_command_t command{};
  };

  std::array<Cell, kCommandQueueCapacity> cells_{};
  std::atomic<std::size_t> enqueue_position_{0u};
  std::atomic<std::size_t> dequeue_position_{0u};
};

#if defined(_WIN32)
constexpr ma_backend kDesktopBackends[] = {ma_backend_wasapi};
#elif defined(__APPLE__)
constexpr ma_backend kDesktopBackends[] = {ma_backend_coreaudio};
#else
constexpr ma_backend kDesktopBackends[] = {ma_backend_pulseaudio, ma_backend_alsa, ma_backend_jack};
#endif

std::string encode_device_id(const ma_device_id& id) {
  static constexpr char kHex[] = "0123456789abcdef";
  const auto* bytes = reinterpret_cast<const unsigned char*>(&id);
  std::string encoded(sizeof(id) * 2u, '0');
  for (std::size_t index = 0u; index < sizeof(id); ++index) {
    encoded[index * 2u] = kHex[bytes[index] >> 4u];
    encoded[index * 2u + 1u] = kHex[bytes[index] & 0x0fu];
  }
  return encoded;
}

int decode_nibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool decode_device_id(const std::string& encoded, ma_device_id& id) {
  if (encoded.size() != sizeof(id) * 2u) return false;
  auto* bytes = reinterpret_cast<unsigned char*>(&id);
  std::memset(bytes, 0, sizeof(id));
  for (std::size_t index = 0u; index < sizeof(id); ++index) {
    const int high = decode_nibble(encoded[index * 2u]);
    const int low = decode_nibble(encoded[index * 2u + 1u]);
    if (high < 0 || low < 0) return false;
    bytes[index] = static_cast<unsigned char>((high << 4) | low);
  }
  return true;
}

bool name_suggests_bluetooth(const std::string& name) {
  std::string lower(name);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return lower.find("bluetooth") != std::string::npos || lower.find("a2dp") != std::string::npos;
}

mol_command_t make_command(mol_command_type_t type) {
  mol_command_t command{};
  command.struct_size = static_cast<std::uint32_t>(sizeof(command));
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

}  // namespace

class AudioRuntime::Impl {
 public:
  bool start(bool null_backend, const std::string& device_id, std::string& error);
  void stop();
  bool initialize_context(bool null_backend, std::string& error);
  bool initialize_device(const std::string& device_id, std::string& error);
  void shutdown_device(bool shutdown_engine = true);
  void drain_commands() noexcept;
  mol_result_t synchronize(mol_engine_state_t* state, const molseq::SequenceDocument* load,
                           molseq::SequenceDocument* recording);
  static void data_callback(ma_device* device, void* output, const void* input,
                            ma_uint32 frame_count) noexcept;
  static void notification_callback(const ma_device_notification* notification) noexcept;

  mutable std::mutex control_mutex;
  ma_context context{};
  ma_device device{};
  bool context_initialized = false;
  bool device_initialized = false;
  bool device_started = false;
  bool null_backend = false;
  std::unique_ptr<EngineMemory> memory = std::make_unique<EngineMemory>();
  std::unique_ptr<PhysicalInputAdapter> physical_input = make_physical_input_adapter();
  mol_engine_t* engine = nullptr;
  CommandQueue commands;
  molcontrol::AudioStatus status;
  std::string input_id;
  std::atomic<bool> shutdown_requested{false};
  std::atomic<bool> stopping{false};
  std::atomic<std::uint64_t> callbacks{0u};
  std::atomic<std::uint64_t> rendered_frames{0u};
  std::atomic<std::uint64_t> render_failures{0u};
  std::atomic<std::uint64_t> non_finite_samples{0u};
  std::atomic<std::uint64_t> underruns{0u};
  std::atomic<std::uint64_t> dropped_commands{0u};
  std::atomic<std::uint64_t> device_notifications{0u};
  std::atomic<std::uint64_t> device_reroutes{0u};
  std::atomic<std::uint64_t> input_events{0u};
};

bool AudioRuntime::Impl::initialize_context(bool use_null_backend, std::string& error) {
  ma_context_config config = ma_context_config_init();
  config.threadPriority = ma_thread_priority_realtime;
  ma_result result = MA_ERROR;
  if (use_null_backend) {
    constexpr ma_backend kNullBackend = ma_backend_null;
    result = ma_context_init(&kNullBackend, 1u, &config, &context);
  } else {
    result = ma_context_init(
        kDesktopBackends,
        static_cast<ma_uint32>(sizeof(kDesktopBackends) / sizeof(kDesktopBackends[0])), &config,
        &context);
  }
  if (result != MA_SUCCESS) {
    error = std::string("audio context initialization failed: ") + ma_result_description(result);
    return false;
  }
  context_initialized = true;
  null_backend = use_null_backend;
  return true;
}

bool AudioRuntime::Impl::initialize_device(const std::string& device_id, std::string& error) {
  const bool had_engine = engine != nullptr;
  ma_device_id selected_id{};
  if (!device_id.empty() && device_id != "default" && !decode_device_id(device_id, selected_id)) {
    error = "invalid output device identifier";
    return false;
  }

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.sampleRate = engine == nullptr ? 0u : status.sample_rate;
  config.periodSizeInFrames = kRequestedPeriodFrames;
  config.periods = kRequestedPeriods;
  config.performanceProfile = ma_performance_profile_low_latency;
  config.noPreSilencedOutputBuffer = MA_TRUE;
  config.noFixedSizedCallback = MA_TRUE;
  config.dataCallback = data_callback;
  config.notificationCallback = notification_callback;
  config.pUserData = this;
  config.playback.pDeviceID = device_id.empty() || device_id == "default" ? nullptr : &selected_id;
  config.playback.format = ma_format_f32;
  config.playback.channels = kChannelCount;
  config.playback.shareMode = ma_share_mode_shared;
#if defined(_WIN32)
  config.wasapi.usage = ma_wasapi_usage_pro_audio;
#endif

  ma_result audio_result = ma_device_init(&context, &config, &device);
  if (audio_result != MA_SUCCESS) {
    error =
        std::string("audio output initialization failed: ") + ma_result_description(audio_result);
    return false;
  }
  device_initialized = true;

  if (engine == nullptr) {
    mol_engine_config_t engine_config = mol_engine_config_default();
    engine_config.sample_rate = device.sampleRate;
    engine_config.channel_count = kChannelCount;
    const std::size_t required = mol_engine_query_memory(&engine_config);
    if (required == 0u || required > memory->bytes.size()) {
      error = "engine memory requirement exceeds the fixed desktop arena";
      shutdown_device();
      return false;
    }
    const mol_result_t engine_result =
        mol_engine_init(memory->bytes.data(), memory->bytes.size(), &engine_config, &engine);
    if (engine_result != MOL_OK) {
      error = std::string("engine initialization failed: ") + mol_result_string(engine_result);
      shutdown_device();
      return false;
    }
  } else if (device.sampleRate != status.sample_rate) {
    error = "selected output cannot use the active engine sample rate";
    ma_device_uninit(&device);
    device_initialized = false;
    return false;
  }

  status.available = true;
  status.backend = ma_get_backend_name(context.backend);
  status.channel_count = kChannelCount;
  status.device_id = device_id.empty() ? "default" : device_id;
  status.device_name = device.playback.name;
  status.period_frames = device.playback.internalPeriodSizeInFrames;
  status.periods = device.playback.internalPeriods;
  status.sample_rate = device.sampleRate;
  status.null_sink = null_backend;
  status.low_latency_requested = true;
  status.estimated_latency_ms = device.sampleRate == 0u
                                    ? 0.0
                                    : static_cast<double>(status.period_frames) *
                                          static_cast<double>(status.periods) * 1000.0 /
                                          static_cast<double>(device.sampleRate);

  audio_result = ma_device_start(&device);
  if (audio_result != MA_SUCCESS) {
    error = std::string("audio output start failed: ") + ma_result_description(audio_result);
    shutdown_device(!had_engine);
    return false;
  }
  device_started = true;
  return true;
}

bool AudioRuntime::Impl::start(bool use_null_backend, const std::string& device_id,
                               std::string& error) {
  std::lock_guard<std::mutex> lock(control_mutex);
  if (context_initialized || device_initialized) {
    error = "audio runtime is already started";
    return false;
  }
  shutdown_requested.store(false, std::memory_order_release);
  stopping.store(false, std::memory_order_release);
  if (!initialize_context(use_null_backend, error)) return false;
  if (initialize_device(device_id, error)) return true;
  ma_context_uninit(&context);
  context_initialized = false;
  if (use_null_backend) return false;

  std::string fallback_error;
  if (!initialize_context(true, fallback_error) || !initialize_device("default", fallback_error)) {
    error += "; null fallback failed: " + fallback_error;
    return false;
  }
  error = "hardware output unavailable; using null backend";
  return true;
}

void AudioRuntime::Impl::shutdown_device(bool shutdown_engine) {
  if (device_started) {
    stopping.store(true, std::memory_order_release);
    (void)ma_device_stop(&device);
    device_started = false;
  }
  if (shutdown_engine && engine != nullptr) {
    drain_commands();
    mol_command_t silence = make_command(MOL_COMMAND_ALL_SOUND_OFF);
    (void)mol_engine_submit(engine, &silence);
    std::array<float, 256u> output{};
    (void)mol_engine_render_interleaved_f32(engine, output.data(), 128u, kChannelCount);
    mol_engine_shutdown(engine);
    engine = nullptr;
  }
  if (device_initialized) {
    ma_device_uninit(&device);
    device_initialized = false;
  }
  status.available = false;
}

void AudioRuntime::Impl::stop() {
  if (physical_input != nullptr) physical_input->detach();
  input_id.clear();
  std::lock_guard<std::mutex> lock(control_mutex);
  shutdown_device();
  if (context_initialized) {
    ma_context_uninit(&context);
    context_initialized = false;
  }
}

void AudioRuntime::Impl::drain_commands() noexcept {
  if (engine == nullptr) return;
  mol_command_t command{};
  while (commands.pop(command)) {
    mol_result_t result = mol_engine_submit(engine, &command);
    if (result == MOL_ERROR_QUEUE_FULL) {
      std::array<float, 2u> frame{};
      if (mol_engine_render_interleaved_f32(engine, frame.data(), 1u, kChannelCount) == MOL_OK)
        result = mol_engine_submit(engine, &command);
    }
    if (result != MOL_OK) dropped_commands.fetch_add(1u, std::memory_order_relaxed);
  }
}

mol_result_t AudioRuntime::Impl::synchronize(mol_engine_state_t* state,
                                             const molseq::SequenceDocument* load,
                                             molseq::SequenceDocument* recording) {
  std::lock_guard<std::mutex> lock(control_mutex);
  if (engine == nullptr) return MOL_ERROR_INVALID_STATE;
  const bool restart = device_started;
  if (restart) {
    stopping.store(true, std::memory_order_release);
    if (ma_device_stop(&device) != MA_SUCCESS) return MOL_ERROR_IO;
    device_started = false;
  }
  drain_commands();
  std::array<float, 2u> output{};
  mol_result_t result = mol_engine_render_interleaved_f32(engine, output.data(), 1u, kChannelCount);
  if (result == MOL_OK && load != nullptr)
    result = mol_engine_load_sequence(engine, &load->config, load->events.data(),
                                      static_cast<std::uint32_t>(load->events.size()));
  if (result == MOL_OK && recording != nullptr) {
    recording->events.resize(2048u);
    std::uint32_t count = 0u;
    result =
        mol_engine_copy_recording(engine, &recording->config, recording->events.data(),
                                  static_cast<std::uint32_t>(recording->events.size()), &count);
    recording->events.resize(count);
  }
  if (result == MOL_OK && state != nullptr) result = mol_engine_get_state(engine, state);
  if (restart) {
    stopping.store(false, std::memory_order_release);
    if (ma_device_start(&device) == MA_SUCCESS)
      device_started = true;
    else if (result == MOL_OK)
      result = MOL_ERROR_IO;
  }
  return result;
}

void AudioRuntime::Impl::data_callback(ma_device* device_pointer, void* output, const void* input,
                                       ma_uint32 frame_count) noexcept {
  auto* impl = static_cast<Impl*>(device_pointer->pUserData);
  auto* samples = static_cast<float*>(output);
  const std::size_t sample_count = static_cast<std::size_t>(frame_count) * kChannelCount;
  (void)input;
  if (impl == nullptr || samples == nullptr || impl->engine == nullptr) return;

  mol_command_t command{};
  ma_uint32 command_count = 0u;
  while (command_count < kMaximumCommandsPerCallback && impl->commands.pop(command)) {
    if (mol_engine_submit(impl->engine, &command) != MOL_OK)
      impl->dropped_commands.fetch_add(1u, std::memory_order_relaxed);
    ++command_count;
  }

  if (mol_engine_render_interleaved_f32(impl->engine, samples, frame_count, kChannelCount) !=
      MOL_OK) {
    std::memset(samples, 0, sample_count * sizeof(*samples));
    impl->render_failures.fetch_add(1u, std::memory_order_relaxed);
    return;
  }
  std::uint64_t invalid = 0u;
  for (std::size_t index = 0u; index < sample_count; ++index) {
    if (!std::isfinite(samples[index])) {
      samples[index] = 0.0f;
      ++invalid;
    }
  }
  impl->non_finite_samples.fetch_add(invalid, std::memory_order_relaxed);
  impl->rendered_frames.fetch_add(frame_count, std::memory_order_relaxed);
  impl->callbacks.fetch_add(1u, std::memory_order_relaxed);
}

void AudioRuntime::Impl::notification_callback(
    const ma_device_notification* notification) noexcept {
  if (notification == nullptr || notification->pDevice == nullptr) return;
  auto* impl = static_cast<Impl*>(notification->pDevice->pUserData);
  if (impl == nullptr) return;
  impl->device_notifications.fetch_add(1u, std::memory_order_relaxed);
  if (notification->type == ma_device_notification_type_rerouted)
    impl->device_reroutes.fetch_add(1u, std::memory_order_relaxed);
  if (notification->type == ma_device_notification_type_stopped &&
      !impl->stopping.load(std::memory_order_acquire))
    impl->underruns.fetch_add(1u, std::memory_order_relaxed);
}

AudioRuntime::AudioRuntime() : impl_(std::make_unique<Impl>()) {}

AudioRuntime::~AudioRuntime() { stop(); }

bool AudioRuntime::start(bool null_backend, const std::string& device_id, std::string& error) {
  return impl_->start(null_backend, device_id, error);
}

void AudioRuntime::stop() { impl_->stop(); }

mol_result_t AudioRuntime::submit(const mol_command_t& command) {
  if (command.struct_size < sizeof(command) || command.api_version != MOL_API_VERSION)
    return MOL_ERROR_INVALID_ARGUMENT;
  if (!impl_->commands.push(command)) {
    impl_->dropped_commands.fetch_add(1u, std::memory_order_relaxed);
    return MOL_ERROR_QUEUE_FULL;
  }
  return MOL_OK;
}

mol_result_t AudioRuntime::snapshot(mol_engine_state_t& state) {
  return impl_->synchronize(&state, nullptr, nullptr);
}

mol_capability_flags_t AudioRuntime::capabilities() const {
  std::lock_guard<std::mutex> lock(impl_->control_mutex);
  return impl_->engine == nullptr ? 0u : mol_engine_get_capabilities(impl_->engine);
}

molcontrol::RuntimeMetrics AudioRuntime::metrics() const {
  molcontrol::RuntimeMetrics result;
  result.callbacks = impl_->callbacks.load(std::memory_order_relaxed);
  result.rendered_frames = impl_->rendered_frames.load(std::memory_order_relaxed);
  result.render_failures = impl_->render_failures.load(std::memory_order_relaxed);
  result.non_finite_samples = impl_->non_finite_samples.load(std::memory_order_relaxed);
  result.underruns = impl_->underruns.load(std::memory_order_relaxed);
  result.dropped_commands = impl_->dropped_commands.load(std::memory_order_relaxed);
  result.device_notifications = impl_->device_notifications.load(std::memory_order_relaxed);
  result.device_reroutes = impl_->device_reroutes.load(std::memory_order_relaxed);
  result.input_events = impl_->input_events.load(std::memory_order_relaxed);
  return result;
}

std::vector<molcontrol::DeviceInfo> AudioRuntime::input_devices() {
  std::vector<molcontrol::DeviceInfo> result = {{"programmatic", "Programmatic and local IPC input",
                                                 "service", true, impl_->input_id == "programmatic",
                                                 false, false}};
  if (impl_->physical_input != nullptr) {
    std::vector<molcontrol::DeviceInfo> physical = impl_->physical_input->devices();
    result.insert(result.end(), std::make_move_iterator(physical.begin()),
                  std::make_move_iterator(physical.end()));
  }
  return result;
}

mol_result_t AudioRuntime::attach_input(const std::string& id) {
  (void)detach_input();
  if (id == "programmatic") {
    impl_->input_id = id;
    return MOL_OK;
  }
  if (impl_->physical_input == nullptr) return MOL_ERROR_UNSUPPORTED;
  const mol_result_t result =
      impl_->physical_input->attach(id, [impl = impl_.get()](const mol_command_t& command) {
        if (!impl->commands.push(command)) {
          impl->dropped_commands.fetch_add(1u, std::memory_order_relaxed);
          return static_cast<mol_result_t>(MOL_ERROR_QUEUE_FULL);
        }
        impl->input_events.fetch_add(1u, std::memory_order_relaxed);
        return static_cast<mol_result_t>(MOL_OK);
      });
  if (result == MOL_OK) impl_->input_id = id;
  return result;
}

mol_result_t AudioRuntime::detach_input() {
  if (impl_->physical_input != nullptr) impl_->physical_input->detach();
  impl_->input_id.clear();
  return MOL_OK;
}

std::string AudioRuntime::active_input_id() const { return impl_->input_id; }

std::vector<molcontrol::DeviceInfo> AudioRuntime::output_devices() {
  std::lock_guard<std::mutex> lock(impl_->control_mutex);
  std::vector<molcontrol::DeviceInfo> result;
  if (!impl_->context_initialized) return result;
  ma_device_info* devices = nullptr;
  ma_uint32 count = 0u;
  if (ma_context_get_devices(&impl_->context, &devices, &count, nullptr, nullptr) != MA_SUCCESS)
    return result;
  result.reserve(count);
  for (ma_uint32 index = 0u; index < count; ++index) {
    molcontrol::DeviceInfo info;
    info.id = encode_device_id(devices[index].id);
    info.name = devices[index].name;
    info.backend = ma_get_backend_name(impl_->context.backend);
    info.is_default = devices[index].isDefault == MA_TRUE;
    info.is_active = info.id == impl_->status.device_id ||
                     (impl_->status.device_id == "default" && info.is_default);
    info.is_bluetooth = name_suggests_bluetooth(info.name);
    result.push_back(std::move(info));
  }
  return result;
}

mol_result_t AudioRuntime::select_output(const std::string& id) {
  std::lock_guard<std::mutex> lock(impl_->control_mutex);
  if (!impl_->context_initialized) return MOL_ERROR_INVALID_STATE;
  const std::string previous_id = impl_->status.device_id;
  impl_->shutdown_device(false);
  impl_->stopping.store(false, std::memory_order_release);
  std::string error;
  if (impl_->initialize_device(id, error)) return MOL_OK;
  impl_->stopping.store(false, std::memory_order_release);
  std::string recovery_error;
  if (!impl_->initialize_device(previous_id, recovery_error) && previous_id != "default") {
    impl_->stopping.store(false, std::memory_order_release);
    (void)impl_->initialize_device("default", recovery_error);
  }
  return MOL_ERROR_IO;
}

molcontrol::AudioStatus AudioRuntime::audio_status() const {
  std::lock_guard<std::mutex> lock(impl_->control_mutex);
  return impl_->status;
}

mol_result_t AudioRuntime::load_sequence(const molseq::SequenceDocument& document) {
  return impl_->synchronize(nullptr, &document, nullptr);
}

mol_result_t AudioRuntime::copy_recording(molseq::SequenceDocument& document) {
  return impl_->synchronize(nullptr, nullptr, &document);
}

bool AudioRuntime::runtime_self_test(std::string& detail) {
  molcontrol::BenchmarkResult result;
  const mol_result_t status = benchmark(4096u, result);
  if (status != MOL_OK || result.non_finite_samples != 0u || result.peak <= 0.0) {
    detail = "isolated render test failed";
    return false;
  }
  detail = "isolated render produced finite, non-silent audio";
  return true;
}

mol_result_t AudioRuntime::benchmark(std::uint64_t frames, molcontrol::BenchmarkResult& result) {
  if (frames == 0u || frames > UINT64_C(1920000)) return MOL_ERROR_INVALID_ARGUMENT;
  const molcontrol::AudioStatus current = audio_status();
  if (current.sample_rate == 0u) return MOL_ERROR_INVALID_STATE;
  auto memory = std::make_unique<EngineMemory>();
  mol_engine_config_t config = mol_engine_config_default();
  config.sample_rate = current.sample_rate;
  config.channel_count = kChannelCount;
  mol_engine_t* engine = nullptr;
  mol_result_t status =
      mol_engine_init(memory->bytes.data(), memory->bytes.size(), &config, &engine);
  if (status != MOL_OK) return status;
  mol_command_t note = make_command(MOL_COMMAND_NOTE_ON);
  note.gesture_id = 1u;
  note.payload.note.note = 60u;
  note.payload.note.velocity = 0.8f;
  status = mol_engine_submit(engine, &note);
  std::array<float, 1024u> output{};
  double peak = 0.0;
  std::uint64_t invalid = 0u;
  std::uint64_t completed = 0u;
  const auto started = std::chrono::steady_clock::now();
  while (status == MOL_OK && completed < frames) {
    const std::uint32_t block =
        static_cast<std::uint32_t>(std::min<std::uint64_t>(512u, frames - completed));
    status = mol_engine_render_interleaved_f32(engine, output.data(), block, kChannelCount);
    for (std::size_t index = 0u; index < static_cast<std::size_t>(block) * kChannelCount; ++index) {
      if (!std::isfinite(output[index]))
        ++invalid;
      else
        peak = std::max(peak, static_cast<double>(std::fabs(output[index])));
    }
    completed += block;
  }
  const double elapsed_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  mol_engine_shutdown(engine);
  if (status != MOL_OK) return status;
  result.frames = frames;
  result.elapsed_ms = elapsed_ms;
  result.peak = peak;
  result.non_finite_samples = invalid;
  result.realtime_ratio =
      elapsed_ms <= std::numeric_limits<double>::epsilon()
          ? 0.0
          : (static_cast<double>(frames) * 1000.0 / static_cast<double>(current.sample_rate)) /
                elapsed_ms;
  return MOL_OK;
}

void AudioRuntime::request_shutdown() {
  impl_->shutdown_requested.store(true, std::memory_order_release);
}

bool AudioRuntime::shutdown_requested() const {
  return impl_->shutdown_requested.load(std::memory_order_acquire);
}

}  // namespace molkeyboardd
