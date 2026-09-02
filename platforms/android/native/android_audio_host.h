// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_ANDROID_AUDIO_HOST_H
#define MOL_ANDROID_AUDIO_HOST_H

#include <atomic>
#include <cstdint>
#include <memory>

#include "mol_platform/audio_runtime.h"
#include "oboe/AudioStream.h"
#include "oboe/AudioStreamBuilder.h"
#include "oboe/AudioStreamCallback.h"

namespace mol::android {

struct AudioStatus {
  std::uint32_t sample_rate;
  std::uint32_t frames_per_burst;
  std::int32_t audio_api;
  std::uint64_t callback_count;
  std::uint64_t rendered_frames;
  std::uint32_t render_failures;
  std::uint32_t non_finite_samples;
  std::int32_t last_error;
  bool active;
  bool disconnected;
};

class AudioHost final : public oboe::AudioStreamDataCallback,
                        public oboe::AudioStreamErrorCallback {
 public:
  AudioHost() = default;
  ~AudioHost() override;
  AudioHost(const AudioHost&) = delete;
  AudioHost& operator=(const AudioHost&) = delete;

  oboe::Result start();
  void stop();
  mol_result_t note_on(std::uint8_t note, float velocity, std::uint64_t gesture_id);
  mol_result_t note_off(std::uint8_t note, std::uint64_t gesture_id);
  [[nodiscard]] AudioStatus status() const noexcept;

  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audio_data,
                                        std::int32_t frame_count) override;
  void onErrorBeforeClose(oboe::AudioStream* stream, oboe::Result error) override;
  void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

 private:
  oboe::Result open_stream(oboe::SharingMode sharing_mode);
  mol_result_t submit_note(std::uint32_t command_type, std::uint8_t note, float velocity,
                           std::uint64_t gesture_id);

  mol_platform_audio_runtime_t runtime_{};
  std::shared_ptr<oboe::AudioStream> stream_;
  std::atomic<std::uint64_t> callback_count_{0};
  std::atomic<std::uint64_t> rendered_frames_{0};
  std::atomic<std::uint32_t> render_failures_{0};
  std::atomic<std::uint32_t> non_finite_samples_{0};
  std::atomic<std::int32_t> last_error_{0};
  std::atomic<std::uint32_t> sample_rate_{0};
  std::atomic<std::uint32_t> frames_per_burst_{0};
  std::atomic<std::int32_t> audio_api_{0};
  std::atomic<bool> runtime_ready_{false};
  std::atomic<bool> active_{false};
  std::atomic<bool> disconnected_{false};
};

}  // namespace mol::android

#endif
