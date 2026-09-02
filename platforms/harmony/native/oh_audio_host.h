// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_HARMONY_OH_AUDIO_HOST_H
#define MOL_HARMONY_OH_AUDIO_HOST_H

#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "mol_platform/audio_runtime.h"

namespace mol::harmony {

struct AudioStatus {
  std::int32_t sample_rate;
  std::int32_t frame_size;
  std::int32_t latency_mode;
  std::uint64_t callback_count;
  std::uint64_t rendered_frames;
  std::uint32_t render_failures;
  std::uint32_t non_finite_samples;
  std::uint32_t underflow_count;
  std::uint32_t route_changes;
  std::uint32_t interruptions;
  std::int32_t last_error;
  bool active;
  bool needs_restart;
  bool fast_path_active;
  bool latency_fallback_used;
};

class AudioHost final {
 public:
  AudioHost() = default;
  ~AudioHost();
  AudioHost(const AudioHost&) = delete;
  AudioHost& operator=(const AudioHost&) = delete;

  OH_AudioStream_Result start();
  void stop();
  OH_AudioStream_Result recover();
  mol_result_t note_on(std::uint8_t note, float velocity, std::uint64_t gesture_id);
  mol_result_t note_off(std::uint8_t note, std::uint64_t gesture_id);
  mol_result_t submit_control(std::uint32_t command_type, std::uint64_t gesture_id,
                              std::int32_t integer_0, std::int32_t integer_1,
                              std::int32_t integer_2, std::int32_t integer_3, float scalar_0,
                              float scalar_1);
  std::uint32_t poll_events(mol_event_t* events, std::uint32_t capacity);
  mol_result_t export_recording(std::uint8_t* bytes, std::size_t capacity, std::size_t* size);
  mol_result_t load_recording(const std::uint8_t* bytes, std::size_t size);
  [[nodiscard]] AudioStatus status();

 private:
  friend OH_AudioData_Callback_Result write_data_callback(OH_AudioRenderer*, void*, void*,
                                                          std::int32_t);
  friend void output_device_change_callback(OH_AudioRenderer*, void*,
                                            OH_AudioStream_DeviceChangeReason);
  friend void interrupt_callback(OH_AudioRenderer*, void*, OH_AudioInterrupt_ForceType,
                                 OH_AudioInterrupt_Hint);
  friend void error_callback(OH_AudioRenderer*, void*, OH_AudioStream_Result);

  mol_result_t submit_note(std::uint32_t command_type, std::uint8_t note, float velocity,
                           std::uint64_t gesture_id);

  mol_platform_audio_runtime_t runtime_{};
  OH_AudioRenderer* renderer_ = nullptr;
  std::atomic<std::uint64_t> callback_count_{0};
  std::atomic<std::uint64_t> rendered_frames_{0};
  std::atomic<std::uint32_t> render_failures_{0};
  std::atomic<std::uint32_t> non_finite_samples_{0};
  std::atomic<std::uint32_t> route_changes_{0};
  std::atomic<std::uint32_t> interruptions_{0};
  std::atomic<std::uint32_t> callbacks_in_flight_{0};
  std::atomic<std::int32_t> sample_rate_{0};
  std::atomic<std::int32_t> frame_size_{0};
  std::atomic<std::int32_t> latency_mode_{AUDIOSTREAM_LATENCY_MODE_NORMAL};
  std::atomic<std::int32_t> last_error_{AUDIOSTREAM_SUCCESS};
  std::atomic<bool> runtime_ready_{false};
  std::atomic<bool> active_{false};
  std::atomic<bool> needs_restart_{false};
  std::atomic<bool> latency_fallback_used_{false};
};

}  // namespace mol::harmony

#endif
