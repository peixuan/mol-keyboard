// SPDX-License-Identifier: Apache-2.0
#include "harmony_ohaudio_sim.h"

#include <new>

struct OH_AudioStreamBuilderStruct {
  std::int32_t sample_rate = 0;
  std::int32_t channel_count = 0;
  OH_AudioStream_SampleFormat sample_format = AUDIOSTREAM_SAMPLE_U8;
  OH_AudioStream_EncodingType encoding = AUDIOSTREAM_ENCODING_TYPE_RAW;
  OH_AudioStream_LatencyMode latency = AUDIOSTREAM_LATENCY_MODE_NORMAL;
  OH_AudioStream_Usage usage = AUDIOSTREAM_USAGE_UNKNOWN;
  OH_AudioInterrupt_Mode interrupt_mode = AUDIOSTREAM_INTERRUPT_MODE_SHARE;
  OH_AudioRenderer_Callbacks callbacks{};
  OH_AudioRenderer_OutputDeviceChangeCallback device_change_callback = nullptr;
  void* user_data = nullptr;
};

struct OH_AudioRendererStruct {
  OH_AudioStreamBuilderStruct settings{};
  bool started = false;
};

namespace {

mol::harmony::test::ApiConfiguration g_configuration{};
mol::harmony::test::ApiSnapshot g_snapshot{};
OH_AudioRenderer* g_renderer = nullptr;

bool callbacks_complete(const OH_AudioRenderer_Callbacks& callbacks) {
  return callbacks.OH_AudioRenderer_OnWriteData != nullptr &&
         callbacks.OH_AudioRenderer_OnStreamEvent != nullptr &&
         callbacks.OH_AudioRenderer_OnInterruptEvent != nullptr &&
         callbacks.OH_AudioRenderer_OnError != nullptr;
}

}  // namespace

namespace mol::harmony::test {

void reset() { configure(ApiConfiguration{}); }

void configure(const ApiConfiguration& configuration) {
  if (g_renderer != nullptr) {
    delete g_renderer;
    g_renderer = nullptr;
  }
  g_configuration = configuration;
  g_snapshot = ApiSnapshot{};
  g_snapshot.requested_sample_format = AUDIOSTREAM_SAMPLE_U8;
  g_snapshot.requested_encoding = AUDIOSTREAM_ENCODING_TYPE_RAW;
  g_snapshot.requested_latency = AUDIOSTREAM_LATENCY_MODE_NORMAL;
  g_snapshot.requested_usage = AUDIOSTREAM_USAGE_UNKNOWN;
  g_snapshot.requested_interrupt_mode = AUDIOSTREAM_INTERRUPT_MODE_SHARE;
}

ApiSnapshot snapshot() {
  g_snapshot.renderer_alive = g_renderer != nullptr;
  return g_snapshot;
}

std::int32_t write(void* audio_data, std::int32_t audio_data_size) {
  if (g_renderer == nullptr ||
      g_renderer->settings.callbacks.OH_AudioRenderer_OnWriteData == nullptr) {
    return AUDIO_DATA_CALLBACK_RESULT_INVALID;
  }
  return g_renderer->settings.callbacks.OH_AudioRenderer_OnWriteData(
      g_renderer, g_renderer->settings.user_data, audio_data, audio_data_size);
}

std::int32_t interrupt(OH_AudioInterrupt_ForceType force_type, OH_AudioInterrupt_Hint hint) {
  if (g_renderer == nullptr ||
      g_renderer->settings.callbacks.OH_AudioRenderer_OnInterruptEvent == nullptr) {
    return AUDIO_DATA_CALLBACK_RESULT_INVALID;
  }
  return g_renderer->settings.callbacks.OH_AudioRenderer_OnInterruptEvent(
      g_renderer, g_renderer->settings.user_data, force_type, hint);
}

void change_output_device(OH_AudioStream_DeviceChangeReason reason) {
  if (g_renderer != nullptr && g_renderer->settings.device_change_callback != nullptr) {
    g_renderer->settings.device_change_callback(g_renderer, g_renderer->settings.user_data, reason);
  }
}

std::int32_t fail(OH_AudioStream_Result error) {
  if (g_renderer == nullptr || g_renderer->settings.callbacks.OH_AudioRenderer_OnError == nullptr) {
    return AUDIO_DATA_CALLBACK_RESULT_INVALID;
  }
  return g_renderer->settings.callbacks.OH_AudioRenderer_OnError(
      g_renderer, g_renderer->settings.user_data, error);
}

}  // namespace mol::harmony::test

extern "C" {

OH_AudioStream_Result OH_AudioStreamBuilder_Create(OH_AudioStreamBuilder** builder,
                                                   OH_AudioStream_Type type) {
  ++g_snapshot.builder_creates;
  if (builder == nullptr || type != AUDIOSTREAM_TYPE_RENDERER) {
    return AUDIOSTREAM_ERROR_INVALID_PARAM;
  }
  *builder = nullptr;
  if (g_configuration.create_result != AUDIOSTREAM_SUCCESS) {
    return g_configuration.create_result;
  }
  *builder = new (std::nothrow) OH_AudioStreamBuilder();
  return *builder == nullptr ? AUDIOSTREAM_ERROR_SYSTEM : AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_Destroy(OH_AudioStreamBuilder* builder) {
  if (builder == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  ++g_snapshot.builder_destroys;
  delete builder;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetSamplingRate(OH_AudioStreamBuilder* builder,
                                                            std::int32_t rate) {
  if (builder == nullptr || rate <= 0) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->sample_rate = rate;
  g_snapshot.requested_sample_rate = rate;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetChannelCount(OH_AudioStreamBuilder* builder,
                                                            std::int32_t channel_count) {
  if (builder == nullptr || channel_count <= 0) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->channel_count = channel_count;
  g_snapshot.requested_channel_count = channel_count;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetSampleFormat(OH_AudioStreamBuilder* builder,
                                                            OH_AudioStream_SampleFormat format) {
  if (builder == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->sample_format = format;
  g_snapshot.requested_sample_format = format;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetEncodingType(OH_AudioStreamBuilder* builder,
                                                            OH_AudioStream_EncodingType encoding) {
  if (builder == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->encoding = encoding;
  g_snapshot.requested_encoding = encoding;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetLatencyMode(OH_AudioStreamBuilder* builder,
                                                           OH_AudioStream_LatencyMode latency) {
  if (builder == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->latency = latency;
  g_snapshot.requested_latency = latency;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererInfo(OH_AudioStreamBuilder* builder,
                                                            OH_AudioStream_Usage usage) {
  if (builder == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->usage = usage;
  g_snapshot.requested_usage = usage;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererInterruptMode(OH_AudioStreamBuilder* builder,
                                                                     OH_AudioInterrupt_Mode mode) {
  if (builder == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  builder->interrupt_mode = mode;
  g_snapshot.requested_interrupt_mode = mode;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererCallback(
    OH_AudioStreamBuilder* builder, OH_AudioRenderer_Callbacks callbacks, void* user_data) {
  if (builder == nullptr || user_data == nullptr || !callbacks_complete(callbacks)) {
    return AUDIOSTREAM_ERROR_INVALID_PARAM;
  }
  builder->callbacks = callbacks;
  builder->user_data = user_data;
  g_snapshot.renderer_callbacks_configured = true;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererOutputDeviceChangeCallback(
    OH_AudioStreamBuilder* builder, OH_AudioRenderer_OutputDeviceChangeCallback callback,
    void* user_data) {
  if (builder == nullptr || callback == nullptr || user_data == nullptr) {
    return AUDIOSTREAM_ERROR_INVALID_PARAM;
  }
  builder->device_change_callback = callback;
  builder->user_data = user_data;
  g_snapshot.device_change_callback_configured = true;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_GenerateRenderer(OH_AudioStreamBuilder* builder,
                                                             OH_AudioRenderer** renderer) {
  ++g_snapshot.renderer_generates;
  if (builder == nullptr || renderer == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *renderer = nullptr;
  if (g_configuration.generate_result != AUDIOSTREAM_SUCCESS) {
    return g_configuration.generate_result;
  }
  if (g_configuration.reject_fast_latency && builder->latency == AUDIOSTREAM_LATENCY_MODE_FAST) {
    return AUDIOSTREAM_ERROR_ILLEGAL_STATE;
  }
  auto* created = new (std::nothrow) OH_AudioRenderer();
  if (created == nullptr) return AUDIOSTREAM_ERROR_SYSTEM;
  created->settings = *builder;
  *renderer = created;
  g_renderer = created;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_Release(OH_AudioRenderer* renderer) {
  if (renderer == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  ++g_snapshot.renderer_releases;
  if (g_renderer == renderer) g_renderer = nullptr;
  delete renderer;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_Start(OH_AudioRenderer* renderer) {
  ++g_snapshot.renderer_starts;
  if (renderer == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  if (g_configuration.start_result != AUDIOSTREAM_SUCCESS) {
    return g_configuration.start_result;
  }
  renderer->started = true;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_Stop(OH_AudioRenderer* renderer) {
  if (renderer == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  ++g_snapshot.renderer_stops;
  renderer->started = false;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_GetSamplingRate(OH_AudioRenderer* renderer,
                                                       std::int32_t* rate) {
  if (renderer == nullptr || rate == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *rate = g_configuration.reported_sample_rate;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_GetChannelCount(OH_AudioRenderer* renderer,
                                                       std::int32_t* channel_count) {
  if (renderer == nullptr || channel_count == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *channel_count = g_configuration.reported_channel_count;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_GetSampleFormat(OH_AudioRenderer* renderer,
                                                       OH_AudioStream_SampleFormat* sample_format) {
  if (renderer == nullptr || sample_format == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *sample_format = g_configuration.reported_sample_format;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_GetLatencyMode(OH_AudioRenderer* renderer,
                                                      OH_AudioStream_LatencyMode* latency_mode) {
  if (renderer == nullptr || latency_mode == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *latency_mode = renderer->settings.latency;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_GetFrameSizeInCallback(OH_AudioRenderer* renderer,
                                                              std::int32_t* frame_size) {
  if (renderer == nullptr || frame_size == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *frame_size = g_configuration.reported_frame_size;
  return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioRenderer_GetUnderflowCount(OH_AudioRenderer* renderer,
                                                         std::uint32_t* count) {
  if (renderer == nullptr || count == nullptr) return AUDIOSTREAM_ERROR_INVALID_PARAM;
  *count = g_configuration.underflow_count;
  return AUDIOSTREAM_SUCCESS;
}

}  // extern "C"
