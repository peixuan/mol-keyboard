// SPDX-License-Identifier: Apache-2.0
#include "oh_audio_host.h"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace mol::harmony {
namespace {

constexpr std::int32_t kRequestedSampleRate = 48000;
constexpr std::int32_t kChannelCount = 2;
constexpr std::int32_t kBytesPerFrame = kChannelCount * static_cast<std::int32_t>(sizeof(float));

void record_result(std::atomic<std::int32_t>& destination, OH_AudioStream_Result result) {
  destination.store(static_cast<std::int32_t>(result), std::memory_order_release);
}

}  // namespace

OH_AudioData_Callback_Result write_data_callback(OH_AudioRenderer* renderer, void* user_data,
                                                 void* audio_data, std::int32_t audio_data_size) {
  (void)renderer;
  auto* host = static_cast<AudioHost*>(user_data);
  if (host == nullptr || audio_data == nullptr || audio_data_size <= 0) {
    return AUDIO_DATA_CALLBACK_RESULT_INVALID;
  }
  if (!host->active_.load(std::memory_order_acquire) ||
      !host->runtime_ready_.load(std::memory_order_acquire)) {
    std::memset(audio_data, 0, static_cast<std::size_t>(audio_data_size));
    return AUDIO_DATA_CALLBACK_RESULT_VALID;
  }
  if (audio_data_size % kBytesPerFrame != 0) {
    std::memset(audio_data, 0, static_cast<std::size_t>(audio_data_size));
    host->render_failures_.fetch_add(1U, std::memory_order_relaxed);
    return AUDIO_DATA_CALLBACK_RESULT_VALID;
  }

  const std::uint32_t frame_count = static_cast<std::uint32_t>(audio_data_size / kBytesPerFrame);
  auto* output = static_cast<float*>(audio_data);
  const mol_result_t result = mol_platform_audio_render_f32(&host->runtime_, output, frame_count);
  if (result != MOL_OK) {
    std::memset(audio_data, 0, static_cast<std::size_t>(audio_data_size));
    host->render_failures_.fetch_add(1U, std::memory_order_relaxed);
  } else {
    std::uint32_t non_finite = 0U;
    const std::size_t sample_count = static_cast<std::size_t>(frame_count) * kChannelCount;
    for (std::size_t index = 0; index < sample_count; ++index) {
      if (!std::isfinite(output[index])) {
        output[index] = 0.0F;
        ++non_finite;
      }
    }
    host->non_finite_samples_.fetch_add(non_finite, std::memory_order_relaxed);
  }
  host->callback_count_.fetch_add(1U, std::memory_order_relaxed);
  host->rendered_frames_.fetch_add(frame_count, std::memory_order_relaxed);
  return AUDIO_DATA_CALLBACK_RESULT_VALID;
}

void output_device_change_callback(OH_AudioRenderer* renderer, void* user_data,
                                   OH_AudioStream_DeviceChangeReason reason) {
  (void)renderer;
  (void)reason;
  auto* host = static_cast<AudioHost*>(user_data);
  if (host != nullptr) {
    host->route_changes_.fetch_add(1U, std::memory_order_relaxed);
    host->active_.store(false, std::memory_order_release);
    host->needs_restart_.store(true, std::memory_order_release);
  }
}

void interrupt_callback(OH_AudioRenderer* renderer, void* user_data,
                        OH_AudioInterrupt_ForceType force_type, OH_AudioInterrupt_Hint hint) {
  (void)renderer;
  auto* host = static_cast<AudioHost*>(user_data);
  if (host == nullptr) {
    return;
  }
  host->interruptions_.fetch_add(1U, std::memory_order_relaxed);
  if (force_type == AUDIOSTREAM_INTERRUPT_FORCE &&
      (hint == AUDIOSTREAM_INTERRUPT_HINT_PAUSE || hint == AUDIOSTREAM_INTERRUPT_HINT_STOP)) {
    host->active_.store(false, std::memory_order_release);
    host->needs_restart_.store(true, std::memory_order_release);
  }
  if (hint == AUDIOSTREAM_INTERRUPT_HINT_RESUME) {
    host->needs_restart_.store(true, std::memory_order_release);
  }
}

void error_callback(OH_AudioRenderer* renderer, void* user_data, OH_AudioStream_Result error) {
  (void)renderer;
  auto* host = static_cast<AudioHost*>(user_data);
  if (host != nullptr) {
    record_result(host->last_error_, error);
    host->active_.store(false, std::memory_order_release);
    host->needs_restart_.store(true, std::memory_order_release);
  }
}

AudioHost::~AudioHost() { stop(); }

OH_AudioStream_Result AudioHost::start() {
  stop();
  OH_AudioStreamBuilder* builder = nullptr;
  OH_AudioStream_Result result = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetSamplingRate(builder, kRequestedSampleRate);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetChannelCount(builder, kChannelCount);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_F32LE);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_FAST);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MUSIC);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result =
        OH_AudioStreamBuilder_SetRendererInterruptMode(builder, AUDIOSTREAM_INTERRUPT_MODE_SHARE);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, write_data_callback, this);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererOutputDeviceChangeCallback(
        builder, output_device_change_callback, this);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererInterruptCallback(builder, interrupt_callback, this);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererErrorCallback(builder, error_callback, this);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_GenerateRenderer(builder, &renderer_);
  }
  if (builder != nullptr) {
    const OH_AudioStream_Result destroy_result = OH_AudioStreamBuilder_Destroy(builder);
    if (result == AUDIOSTREAM_SUCCESS && destroy_result != AUDIOSTREAM_SUCCESS) {
      result = destroy_result;
    }
  }
  if (result != AUDIOSTREAM_SUCCESS || renderer_ == nullptr) {
    record_result(last_error_, result);
    stop();
    return result;
  }

  std::int32_t sample_rate = 0;
  std::int32_t channel_count = 0;
  std::int32_t frame_size = 0;
  OH_AudioStream_SampleFormat sample_format = AUDIOSTREAM_SAMPLE_S16LE;
  OH_AudioStream_LatencyMode latency_mode = AUDIOSTREAM_LATENCY_MODE_NORMAL;
  result = OH_AudioRenderer_GetSamplingRate(renderer_, &sample_rate);
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioRenderer_GetChannelCount(renderer_, &channel_count);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioRenderer_GetSampleFormat(renderer_, &sample_format);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioRenderer_GetFrameSizeInCallback(renderer_, &frame_size);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioRenderer_GetLatencyMode(renderer_, &latency_mode);
  }
  if (result != AUDIOSTREAM_SUCCESS || sample_rate <= 0 || channel_count != kChannelCount ||
      sample_format != AUDIOSTREAM_SAMPLE_F32LE) {
    if (result == AUDIOSTREAM_SUCCESS) {
      result = AUDIOSTREAM_ERROR_UNSUPPORTED_FORMAT;
    }
    record_result(last_error_, result);
    stop();
    return result;
  }

  const mol_result_t engine_result =
      mol_platform_audio_init(&runtime_, static_cast<std::uint32_t>(sample_rate),
                              static_cast<std::uint32_t>(channel_count));
  if (engine_result != MOL_OK) {
    record_result(last_error_, AUDIOSTREAM_ERROR_SYSTEM);
    stop();
    return AUDIOSTREAM_ERROR_SYSTEM;
  }
  sample_rate_.store(sample_rate, std::memory_order_release);
  frame_size_.store(frame_size, std::memory_order_release);
  latency_mode_.store(static_cast<std::int32_t>(latency_mode), std::memory_order_release);
  runtime_ready_.store(true, std::memory_order_release);
  active_.store(true, std::memory_order_release);
  result = OH_AudioRenderer_Start(renderer_);
  if (result != AUDIOSTREAM_SUCCESS) {
    record_result(last_error_, result);
    stop();
    return result;
  }
  needs_restart_.store(false, std::memory_order_release);
  record_result(last_error_, AUDIOSTREAM_SUCCESS);
  return AUDIOSTREAM_SUCCESS;
}

void AudioHost::stop() {
  active_.store(false, std::memory_order_release);
  if (renderer_ != nullptr) {
    (void)OH_AudioRenderer_Stop(renderer_);
    (void)OH_AudioRenderer_Release(renderer_);
    renderer_ = nullptr;
  }
  runtime_ready_.store(false, std::memory_order_release);
  mol_platform_audio_shutdown(&runtime_);
  sample_rate_.store(0, std::memory_order_release);
  frame_size_.store(0, std::memory_order_release);
}

OH_AudioStream_Result AudioHost::recover() {
  if (!needs_restart_.load(std::memory_order_acquire)) {
    return AUDIOSTREAM_SUCCESS;
  }
  return start();
}

mol_result_t AudioHost::submit_note(std::uint32_t command_type, std::uint8_t note, float velocity,
                                    std::uint64_t gesture_id) {
  mol_command_t command{};
  if (!runtime_ready_.load(std::memory_order_acquire) || note > 127U || !std::isfinite(velocity) ||
      velocity < 0.0F || velocity > 1.0F) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command.struct_size = sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = command_type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = gesture_id;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  return mol_platform_audio_submit(&runtime_, &command);
}

mol_result_t AudioHost::note_on(std::uint8_t note, float velocity, std::uint64_t gesture_id) {
  return submit_note(MOL_COMMAND_NOTE_ON, note, velocity, gesture_id);
}

mol_result_t AudioHost::note_off(std::uint8_t note, std::uint64_t gesture_id) {
  return submit_note(MOL_COMMAND_NOTE_OFF, note, 0.0F, gesture_id);
}

AudioStatus AudioHost::status() {
  std::uint32_t underflows = 0U;
  if (renderer_ != nullptr) {
    (void)OH_AudioRenderer_GetUnderflowCount(renderer_, &underflows);
  }
  return AudioStatus{sample_rate_.load(std::memory_order_acquire),
                     frame_size_.load(std::memory_order_acquire),
                     latency_mode_.load(std::memory_order_acquire),
                     callback_count_.load(std::memory_order_relaxed),
                     rendered_frames_.load(std::memory_order_relaxed),
                     render_failures_.load(std::memory_order_relaxed),
                     non_finite_samples_.load(std::memory_order_relaxed),
                     underflows,
                     route_changes_.load(std::memory_order_relaxed),
                     interruptions_.load(std::memory_order_relaxed),
                     last_error_.load(std::memory_order_acquire),
                     active_.load(std::memory_order_acquire),
                     needs_restart_.load(std::memory_order_acquire)};
}

}  // namespace mol::harmony
