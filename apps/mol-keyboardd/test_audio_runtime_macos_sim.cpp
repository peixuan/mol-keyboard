// SPDX-License-Identifier: Apache-2.0
#include "miniaudio.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "audio_runtime.hpp"
#include "physical_input.hpp"

namespace {

std::atomic<int> context_initializations{0};
std::atomic<int> device_starts{0};
std::atomic<ma_backend> selected_backend{ma_backend_null};
std::atomic<ma_device*> active_device{nullptr};

[[noreturn]] void fail(const char* message) {
  std::fprintf(stderr, "macOS audio simulation failure: %s\n", message);
  std::exit(1);
}

void require(bool condition, const char* message) {
  if (!condition) fail(message);
}

bool wait_for_start_count(int expected) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    if (device_starts.load(std::memory_order_acquire) >= expected) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

void notify(ma_device_notification_type type) {
  ma_device* device = active_device.load(std::memory_order_acquire);
  require(device != nullptr && device->notificationCallback != nullptr,
          "notification target must be active");
  const ma_device_notification notification{device, type};
  device->notificationCallback(&notification);
}

}  // namespace

extern "C" {

ma_context_config ma_context_config_init() {
  ma_context_config config{};
  config.threadPriority = ma_thread_priority_realtime;
  return config;
}

ma_result ma_context_init(const ma_backend* backends, ma_uint32 backend_count,
                          const ma_context_config*, ma_context* context) {
  if (backends == nullptr || backend_count == 0u || context == nullptr) return MA_ERROR;
  context->backend = backends[0];
  selected_backend.store(backends[0], std::memory_order_release);
  context_initializations.fetch_add(1, std::memory_order_relaxed);
  return MA_SUCCESS;
}

void ma_context_uninit(ma_context*) {}

ma_device_config ma_device_config_init(ma_device_type) { return {}; }

ma_result ma_device_init(ma_context*, const ma_device_config* config, ma_device* device) {
  if (config == nullptr || device == nullptr || config->playback.channels != 2u ||
      config->playback.format != ma_format_f32)
    return MA_ERROR;
  device->sampleRate = config->sampleRate == 0u ? 48000u : config->sampleRate;
  device->pUserData = config->pUserData;
  device->dataCallback = config->dataCallback;
  device->notificationCallback = config->notificationCallback;
  std::snprintf(device->playback.name, sizeof(device->playback.name), "%s",
                "Simulated CoreAudio Output");
  device->playback.internalPeriodSizeInFrames = config->periodSizeInFrames;
  device->playback.internalPeriods = config->periods;
  active_device.store(device, std::memory_order_release);
  return MA_SUCCESS;
}

void ma_device_uninit(ma_device* device) {
  ma_device* expected = device;
  (void)active_device.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

ma_result ma_device_start(ma_device* device) {
  if (device == nullptr || device->dataCallback == nullptr) return MA_ERROR;
  std::array<float, 256u> output{};
  device->dataCallback(device, output.data(), nullptr, 128u);
  device_starts.fetch_add(1, std::memory_order_release);
  return MA_SUCCESS;
}

ma_result ma_device_stop(ma_device*) { return MA_SUCCESS; }

ma_result ma_context_get_devices(ma_context*, ma_device_info** playback_devices,
                                 ma_uint32* playback_count, ma_device_info**, ma_uint32*) {
  static ma_device_info device{};
  device.id.bytes[0] = 0x42u;
  std::snprintf(device.name, sizeof(device.name), "%s", "Simulated CoreAudio Output");
  device.isDefault = MA_TRUE;
  if (playback_devices != nullptr) *playback_devices = &device;
  if (playback_count != nullptr) *playback_count = 1u;
  return MA_SUCCESS;
}

const char* ma_get_backend_name(ma_backend backend) {
  return backend == ma_backend_coreaudio ? "Core Audio" : "Null";
}

const char* ma_result_description(ma_result result) {
  return result == MA_SUCCESS ? "success" : "simulated error";
}

}  // extern "C"

namespace molkeyboardd {

std::unique_ptr<PhysicalInputAdapter> make_physical_input_adapter() { return nullptr; }

}  // namespace molkeyboardd

int main() {
  molkeyboardd::AudioRuntime runtime;
  std::string warning;
  require(runtime.start(false, "default", warning), "CoreAudio runtime startup");
  require(warning.empty(), "CoreAudio startup must not fall back");
  require(selected_backend.load() == ma_backend_coreaudio, "CoreAudio backend selection");

  const molcontrol::AudioStatus status = runtime.audio_status();
  require(status.available && !status.null_sink, "hardware output status");
  require(status.backend == "Core Audio", "reported CoreAudio backend");
  require(status.device_name == "Simulated CoreAudio Output", "reported output name");
  require(status.sample_rate == 48000u && status.period_frames == 128u && status.periods == 3u,
          "effective CoreAudio stream configuration");
  require(device_starts.load() == 1, "initial callback start");

  const std::vector<molcontrol::DeviceInfo> outputs = runtime.output_devices();
  require(outputs.size() == 1u && outputs[0].is_default && outputs[0].is_active,
          "CoreAudio device enumeration");
  require(runtime.select_output(outputs[0].id) == MOL_OK, "CoreAudio output selection");
  require(device_starts.load() == 2, "selected output restart");

  notify(ma_device_notification_type_rerouted);
  notify(ma_device_notification_type_stopped);
  require(wait_for_start_count(3), "stopped device recovery");
  const molcontrol::RuntimeMetrics metrics = runtime.metrics();
  require(metrics.callbacks >= 3u && metrics.rendered_frames >= 384u,
          "CoreAudio callback accounting");
  require(metrics.device_notifications == 2u && metrics.device_reroutes == 1u &&
              metrics.underruns == 1u,
          "CoreAudio notification accounting");
  require(metrics.render_failures == 0u && metrics.non_finite_samples == 0u,
          "finite CoreAudio rendering");
  require(context_initializations.load() == 1, "recovery reuses the CoreAudio context");

  runtime.stop();
  require(active_device.load() == nullptr, "CoreAudio device cleanup");
  std::puts("macOS CoreAudio simulation passed");
  return 0;
}
