// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "miniaudio.h"
#include "mol/mol.h"

namespace {

constexpr std::uint32_t kChannelCount = 2;
constexpr std::uint32_t kMaxVoices = 8;
constexpr std::uint32_t kCommandCapacity = 32;
constexpr std::uint32_t kEventCapacity = 32;
constexpr std::size_t kEngineMemoryBytes = 65536;
constexpr ma_uint32 kRequestedPeriodFrames = 128;
constexpr ma_uint32 kRequestedPeriods = 3;
constexpr double kMinimumDurationSeconds = 1.0;
constexpr double kMaximumDurationSeconds = 3600.0;

struct Options {
  bool help = false;
  bool list_devices = false;
  bool null_backend = false;
  double duration_seconds = 2.0;
  int note = 60;
  float velocity = 0.5F;
  std::string device_id;
};

struct PlaybackState {
  alignas(std::max_align_t) unsigned char engine_memory[kEngineMemoryBytes]{};
  mol_engine_t* engine = nullptr;
  std::uint32_t sample_rate = 0;
  std::uint64_t rendered_frames = 0;
  std::uint32_t callback_count = 0;
  std::uint32_t render_failures = 0;
  std::uint32_t non_finite_samples = 0;
  std::uint32_t crossings = 0;
  float previous_sample = 0.0F;
  float peak = 0.0F;
  std::atomic<std::uint32_t> notification_count{0};
  std::atomic<std::uint32_t> reroute_count{0};
  std::atomic<bool> stop_requested{false};
  std::atomic<bool> external_stop{false};
};

#if defined(_WIN32)
constexpr ma_backend kDesktopBackends[] = {ma_backend_wasapi};
#elif defined(__APPLE__)
constexpr ma_backend kDesktopBackends[] = {ma_backend_coreaudio};
#else
constexpr ma_backend kDesktopBackends[] = {ma_backend_pulseaudio, ma_backend_alsa, ma_backend_jack};
#endif

void print_usage(const char* executable) {
  std::printf(
      "Usage: %s [--list-devices] [--null-backend] [--device-id HEX]\n"
      "          [--duration SECONDS] [--note 0..127] [--velocity 0..1]\n",
      executable);
}

bool parse_double(const char* text, double* value) {
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(text, &end);
  if (errno != 0 || end == text || end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
    return false;
  }
  *value = parsed;
  return true;
}

bool parse_int(const char* text, int* value) {
  char* end = nullptr;
  errno = 0;
  const long parsed = std::strtol(text, &end, 10);
  if (errno != 0 || end == text || end == nullptr || *end != '\0' || parsed < 0L || parsed > 127L) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

bool parse_options(int argc, char** argv, Options* options) {
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--help") {
      options->help = true;
      continue;
    }
    if (argument == "--list-devices") {
      options->list_devices = true;
      continue;
    }
    if (argument == "--null-backend") {
      options->null_backend = true;
      continue;
    }
    if (argument == "--device-id" && index + 1 < argc) {
      options->device_id = argv[++index];
      continue;
    }
    if (argument == "--duration" && index + 1 < argc) {
      if (!parse_double(argv[++index], &options->duration_seconds) ||
          options->duration_seconds < kMinimumDurationSeconds ||
          options->duration_seconds > kMaximumDurationSeconds) {
        std::fprintf(stderr, "Invalid duration; expected 1..3600 seconds\n");
        return false;
      }
      continue;
    }
    if (argument == "--note" && index + 1 < argc) {
      if (!parse_int(argv[++index], &options->note)) {
        std::fprintf(stderr, "Invalid note; expected MIDI note 0..127\n");
        return false;
      }
      continue;
    }
    if (argument == "--velocity" && index + 1 < argc) {
      double velocity = 0.0;
      if (!parse_double(argv[++index], &velocity) || velocity < 0.0 || velocity > 1.0) {
        std::fprintf(stderr, "Invalid velocity; expected 0..1\n");
        return false;
      }
      options->velocity = static_cast<float>(velocity);
      continue;
    }
    std::fprintf(stderr, "Unknown or incomplete argument: %s\n", argument.c_str());
    return false;
  }
  return true;
}

std::string encode_device_id(const ma_device_id& id) {
  static constexpr char kHex[] = "0123456789abcdef";
  const auto* bytes = reinterpret_cast<const unsigned char*>(&id);
  std::string encoded(sizeof(id) * 2U, '0');
  for (std::size_t index = 0; index < sizeof(id); ++index) {
    encoded[index * 2U] = kHex[bytes[index] >> 4U];
    encoded[index * 2U + 1U] = kHex[bytes[index] & 0x0FU];
  }
  return encoded;
}

int decode_hex_nibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

bool decode_device_id(const std::string& encoded, ma_device_id* id) {
  if (encoded.size() != sizeof(*id) * 2U) {
    return false;
  }
  std::memset(id, 0, sizeof(*id));
  auto* bytes = reinterpret_cast<unsigned char*>(id);
  for (std::size_t index = 0; index < sizeof(*id); ++index) {
    const int high = decode_hex_nibble(encoded[index * 2U]);
    const int low = decode_hex_nibble(encoded[index * 2U + 1U]);
    if (high < 0 || low < 0) {
      return false;
    }
    bytes[index] = static_cast<unsigned char>((high << 4) | low);
  }
  return true;
}

void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
  auto* state = static_cast<PlaybackState*>(device->pUserData);
  auto* samples = static_cast<float*>(output);
  const std::size_t sample_count = static_cast<std::size_t>(frame_count) * kChannelCount;
  const std::uint64_t analysis_start = state->sample_rate / 10U;
  const std::uint64_t analysis_end = analysis_start + state->sample_rate * 8U / 10U;
  (void)input;

  if (samples == nullptr || mol_engine_render_interleaved_f32(state->engine, samples, frame_count,
                                                              kChannelCount) != MOL_OK) {
    if (samples != nullptr) {
      std::memset(samples, 0, sample_count * sizeof(*samples));
    }
    ++state->render_failures;
    return;
  }

  for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
    const std::uint64_t absolute_frame = state->rendered_frames + frame;
    for (std::uint32_t channel = 0; channel < kChannelCount; ++channel) {
      float& sample = samples[static_cast<std::size_t>(frame) * kChannelCount + channel];
      if (!std::isfinite(sample)) {
        sample = 0.0F;
        ++state->non_finite_samples;
      }
      state->peak = std::max(state->peak, std::fabs(sample));
    }
    const float left = samples[static_cast<std::size_t>(frame) * kChannelCount];
    if (absolute_frame >= analysis_start && absolute_frame < analysis_end &&
        state->previous_sample <= 0.0F && left > 0.0F) {
      ++state->crossings;
    }
    state->previous_sample = left;
  }
  state->rendered_frames += frame_count;
  ++state->callback_count;
}

void notification_callback(const ma_device_notification* notification) {
  auto* state = static_cast<PlaybackState*>(notification->pDevice->pUserData);
  state->notification_count.fetch_add(1U, std::memory_order_relaxed);
  if (notification->type == ma_device_notification_type_rerouted) {
    state->reroute_count.fetch_add(1U, std::memory_order_relaxed);
  } else if (notification->type == ma_device_notification_type_stopped &&
             !state->stop_requested.load(std::memory_order_relaxed)) {
    state->external_stop.store(true, std::memory_order_release);
  }
}

ma_result initialize_context(bool null_backend, ma_context* context) {
  ma_context_config config = ma_context_config_init();
  config.threadPriority = ma_thread_priority_realtime;
  if (null_backend) {
    constexpr ma_backend kNullBackend = ma_backend_null;
    return ma_context_init(&kNullBackend, 1U, &config, context);
  }
  return ma_context_init(
      kDesktopBackends,
      static_cast<ma_uint32>(sizeof(kDesktopBackends) / sizeof(kDesktopBackends[0])), &config,
      context);
}

int list_devices(ma_context* context) {
  ma_device_info* playback_devices = nullptr;
  ma_uint32 playback_count = 0;
  const ma_result result =
      ma_context_get_devices(context, &playback_devices, &playback_count, nullptr, nullptr);
  if (result != MA_SUCCESS) {
    std::fprintf(stderr, "Device enumeration failed: %s\n", ma_result_description(result));
    return 1;
  }
  std::printf("backend=%s playback_devices=%u\n", ma_get_backend_name(context->backend),
              static_cast<unsigned int>(playback_count));
  for (ma_uint32 index = 0; index < playback_count; ++index) {
    const std::string id = encode_device_id(playback_devices[index].id);
    std::printf("index=%u default=%s name=%s id=%s\n", static_cast<unsigned int>(index),
                playback_devices[index].isDefault == MA_TRUE ? "true" : "false",
                playback_devices[index].name, id.c_str());
  }
  return 0;
}

mol_command_t make_note_on(const Options& options) {
  mol_command_t command{};
  command.struct_size = sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = MOL_COMMAND_NOTE_ON;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = 1U;
  command.payload.note.note = static_cast<std::uint8_t>(options.note);
  command.payload.note.velocity = options.velocity;
  return command;
}

int play(const Options& options, ma_context* context) {
  PlaybackState state{};
  ma_device_id selected_id{};
  ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
  ma_device device{};

  if (!options.device_id.empty() && !decode_device_id(options.device_id, &selected_id)) {
    std::fprintf(stderr, "Invalid device ID; copy the complete value from --list-devices\n");
    return 1;
  }

  device_config.sampleRate = 0U;
  device_config.periodSizeInFrames = kRequestedPeriodFrames;
  device_config.periods = kRequestedPeriods;
  device_config.performanceProfile = ma_performance_profile_low_latency;
  device_config.noPreSilencedOutputBuffer = MA_TRUE;
  device_config.noFixedSizedCallback = MA_TRUE;
  device_config.dataCallback = data_callback;
  device_config.notificationCallback = notification_callback;
  device_config.pUserData = &state;
  device_config.playback.pDeviceID = options.device_id.empty() ? nullptr : &selected_id;
  device_config.playback.format = ma_format_f32;
  device_config.playback.channels = kChannelCount;
  device_config.playback.shareMode = ma_share_mode_shared;
#if defined(_WIN32)
  device_config.wasapi.usage = ma_wasapi_usage_pro_audio;
#endif

  ma_result result = ma_device_init(context, &device_config, &device);
  if (result != MA_SUCCESS) {
    std::fprintf(stderr, "Playback device initialization failed: %s\n",
                 ma_result_description(result));
    return 1;
  }

  mol_engine_config_t engine_config = mol_engine_config_default();
  engine_config.sample_rate = device.sampleRate;
  engine_config.channel_count = kChannelCount;
  engine_config.max_voices = kMaxVoices;
  engine_config.command_capacity = kCommandCapacity;
  engine_config.event_capacity = kEventCapacity;
  state.sample_rate = device.sampleRate;
  const std::size_t required_memory = mol_engine_query_memory(&engine_config);
  if (required_memory == 0U || required_memory > sizeof(state.engine_memory)) {
    std::fprintf(stderr, "Engine memory requirement is invalid: %zu bytes\n", required_memory);
    ma_device_uninit(&device);
    return 1;
  }

  mol_result_t engine_result = mol_engine_init(state.engine_memory, sizeof(state.engine_memory),
                                               &engine_config, &state.engine);
  const mol_command_t note_on = make_note_on(options);
  if (engine_result == MOL_OK) {
    engine_result = mol_engine_submit(state.engine, &note_on);
  }
  if (engine_result != MOL_OK) {
    std::fprintf(stderr, "Engine startup failed: %s\n", mol_result_string(engine_result));
    ma_device_uninit(&device);
    return 1;
  }

  const double latency_ms = device.sampleRate == 0U
                                ? 0.0
                                : static_cast<double>(device.playback.internalPeriodSizeInFrames) *
                                      static_cast<double>(device.playback.internalPeriods) *
                                      1000.0 / static_cast<double>(device.sampleRate);
  std::printf(
      "backend=%s device=%s sample_rate=%u callback_hint=%u "
      "native_period_frames=%u native_periods=%u estimated_latency_ms=%.3f\n",
      ma_get_backend_name(context->backend), device.playback.name,
      static_cast<unsigned int>(device.sampleRate),
      static_cast<unsigned int>(kRequestedPeriodFrames),
      static_cast<unsigned int>(device.playback.internalPeriodSizeInFrames),
      static_cast<unsigned int>(device.playback.internalPeriods), latency_ms);

  result = ma_device_start(&device);
  if (result != MA_SUCCESS) {
    std::fprintf(stderr, "Playback start failed: %s\n", ma_result_description(result));
    mol_engine_shutdown(state.engine);
    ma_device_uninit(&device);
    return 1;
  }

  const auto started = std::chrono::steady_clock::now();
  while (!state.external_stop.load(std::memory_order_acquire)) {
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    if (elapsed.count() >= options.duration_seconds) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  state.stop_requested.store(true, std::memory_order_release);
  result = ma_device_stop(&device);
  if (result != MA_SUCCESS) {
    std::fprintf(stderr, "Playback stop failed: %s\n", ma_result_description(result));
  }
  const bool externally_stopped = state.external_stop.load(std::memory_order_acquire);
  ma_device_uninit(&device);

  const std::uint64_t analysis_frames = static_cast<std::uint64_t>(state.sample_rate) * 8U / 10U;
  const double frequency = analysis_frames == 0U ? 0.0
                                                 : static_cast<double>(state.crossings) *
                                                       static_cast<double>(state.sample_rate) /
                                                       static_cast<double>(analysis_frames);
  const double expected_frequency =
      440.0 * std::pow(2.0, (static_cast<double>(options.note) - 69.0) / 12.0);
  std::printf(
      "callbacks=%u frames=%llu frequency_hz=%.4f peak=%.8f "
      "render_failures=%u non_finite=%u notifications=%u reroutes=%u\n",
      static_cast<unsigned int>(state.callback_count),
      static_cast<unsigned long long>(state.rendered_frames), frequency,
      static_cast<double>(state.peak), static_cast<unsigned int>(state.render_failures),
      static_cast<unsigned int>(state.non_finite_samples),
      static_cast<unsigned int>(state.notification_count.load(std::memory_order_relaxed)),
      static_cast<unsigned int>(state.reroute_count.load(std::memory_order_relaxed)));

  mol_engine_shutdown(state.engine);
  if (externally_stopped) {
    std::fprintf(stderr, "Playback device stopped externally; output is now silent\n");
    return 2;
  }
  if (result != MA_SUCCESS || state.callback_count == 0U ||
      state.rendered_frames < static_cast<std::uint64_t>(state.sample_rate) * 9U / 10U ||
      state.peak <= 0.01F || state.render_failures != 0U || state.non_finite_samples != 0U ||
      std::fabs(frequency - expected_frequency) >= 1.0) {
    std::fprintf(stderr, "Realtime audio conformance failed\n");
    return 1;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parse_options(argc, argv, &options)) {
    return 2;
  }
  if (options.help) {
    print_usage(argv[0]);
    return 0;
  }

  ma_context context{};
  const ma_result result = initialize_context(options.null_backend, &context);
  if (result != MA_SUCCESS) {
    std::fprintf(stderr, "Audio context initialization failed: %s\n",
                 ma_result_description(result));
    return 1;
  }

  const int exit_code = options.list_devices ? list_devices(&context) : play(options, &context);
  ma_context_uninit(&context);
  return exit_code;
}
