// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_TESTS_HARMONY_OHAUDIO_SIM_H
#define MOL_TESTS_HARMONY_OHAUDIO_SIM_H

#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>

#include <cstdint>

namespace mol::harmony::test {

struct ApiConfiguration {
  OH_AudioStream_Result create_result = AUDIOSTREAM_SUCCESS;
  OH_AudioStream_Result generate_result = AUDIOSTREAM_SUCCESS;
  OH_AudioStream_Result start_result = AUDIOSTREAM_SUCCESS;
  std::int32_t reported_sample_rate = 48000;
  std::int32_t reported_channel_count = 2;
  std::int32_t reported_frame_size = 192;
  OH_AudioStream_SampleFormat reported_sample_format = AUDIOSTREAM_SAMPLE_S16LE;
  std::uint32_t underflow_count = 0U;
  bool reject_fast_latency = false;
};

struct ApiSnapshot {
  std::uint32_t builder_creates;
  std::uint32_t builder_destroys;
  std::uint32_t renderer_generates;
  std::uint32_t renderer_starts;
  std::uint32_t renderer_stops;
  std::uint32_t renderer_releases;
  std::int32_t requested_sample_rate;
  std::int32_t requested_channel_count;
  OH_AudioStream_SampleFormat requested_sample_format;
  OH_AudioStream_EncodingType requested_encoding;
  OH_AudioStream_LatencyMode requested_latency;
  OH_AudioStream_Usage requested_usage;
  OH_AudioInterrupt_Mode requested_interrupt_mode;
  bool renderer_callbacks_configured;
  bool device_change_callback_configured;
  bool renderer_alive;
};

void reset();
void configure(const ApiConfiguration& configuration);
[[nodiscard]] ApiSnapshot snapshot();

std::int32_t write(void* audio_data, std::int32_t audio_data_size);
std::int32_t interrupt(OH_AudioInterrupt_ForceType force_type, OH_AudioInterrupt_Hint hint);
void change_output_device(OH_AudioStream_DeviceChangeReason reason);
std::int32_t fail(OH_AudioStream_Result error);

}  // namespace mol::harmony::test

#endif
