// SPDX-License-Identifier: Apache-2.0
#include "android_audio_host.h"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace mol::android {
namespace {

constexpr std::uint32_t kChannelCount = 2U;
constexpr std::int32_t kBufferBurstCount = 2;

void silence(float* output, std::int32_t frame_count) noexcept {
  if (output != nullptr && frame_count > 0) {
    std::memset(output, 0, static_cast<std::size_t>(frame_count) * kChannelCount * sizeof(*output));
  }
}

}  // namespace

AudioHost::~AudioHost() { stop(); }

oboe::Result AudioHost::open_stream(oboe::SharingMode sharing_mode) {
  oboe::AudioStreamBuilder builder;
  builder.setDirection(oboe::Direction::Output)
      ->setAudioApi(oboe::AudioApi::Unspecified)
      ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
      ->setSharingMode(sharing_mode)
      ->setUsage(oboe::Usage::Game)
      ->setContentType(oboe::ContentType::Music)
      ->setFormat(oboe::AudioFormat::Float)
      ->setChannelCount(oboe::ChannelCount::Stereo)
      ->setFormatConversionAllowed(true)
      ->setChannelConversionAllowed(true)
      ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
      ->setDataCallback(this)
      ->setErrorCallback(this);
  return builder.openStream(stream_);
}

oboe::Result AudioHost::start() {
  stop();
  disconnected_.store(false, std::memory_order_release);
  last_error_.store(static_cast<std::int32_t>(oboe::Result::OK), std::memory_order_release);

  oboe::Result result = open_stream(oboe::SharingMode::Exclusive);
  if (result != oboe::Result::OK) {
    stream_.reset();
    result = open_stream(oboe::SharingMode::Shared);
  }
  if (result != oboe::Result::OK || stream_ == nullptr) {
    last_error_.store(static_cast<std::int32_t>(result), std::memory_order_release);
    stream_.reset();
    return result;
  }

  const std::int32_t sample_rate = stream_->getSampleRate();
  const std::int32_t channel_count = stream_->getChannelCount();
  const std::int32_t frames_per_burst = stream_->getFramesPerBurst();
  if (sample_rate <= 0 || channel_count != static_cast<std::int32_t>(kChannelCount) ||
      stream_->getFormat() != oboe::AudioFormat::Float) {
    last_error_.store(static_cast<std::int32_t>(oboe::Result::ErrorInvalidFormat),
                      std::memory_order_release);
    (void)stream_->close();
    stream_.reset();
    return oboe::Result::ErrorInvalidFormat;
  }

  const mol_result_t engine_result =
      mol_platform_audio_init(&runtime_, static_cast<std::uint32_t>(sample_rate), kChannelCount);
  if (engine_result != MOL_OK) {
    last_error_.store(-10000 - engine_result, std::memory_order_release);
    (void)stream_->close();
    stream_.reset();
    return oboe::Result::ErrorInternal;
  }

  sample_rate_.store(static_cast<std::uint32_t>(sample_rate), std::memory_order_release);
  frames_per_burst_.store(frames_per_burst > 0 ? static_cast<std::uint32_t>(frames_per_burst) : 0U,
                          std::memory_order_release);
  audio_api_.store(static_cast<std::int32_t>(stream_->getAudioApi()), std::memory_order_release);
  if (frames_per_burst > 0) {
    (void)stream_->setBufferSizeInFrames(frames_per_burst * kBufferBurstCount);
  }
  runtime_ready_.store(true, std::memory_order_release);
  active_.store(true, std::memory_order_release);
  result = stream_->requestStart();
  if (result != oboe::Result::OK) {
    active_.store(false, std::memory_order_release);
    runtime_ready_.store(false, std::memory_order_release);
    last_error_.store(static_cast<std::int32_t>(result), std::memory_order_release);
    (void)stream_->close();
    stream_.reset();
    mol_platform_audio_shutdown(&runtime_);
  }
  return result;
}

void AudioHost::stop() {
  active_.store(false, std::memory_order_release);
  if (stream_ != nullptr) {
    (void)stream_->requestStop();
    (void)stream_->close();
    stream_.reset();
  }
  runtime_ready_.store(false, std::memory_order_release);
  mol_platform_audio_shutdown(&runtime_);
  sample_rate_.store(0U, std::memory_order_release);
  frames_per_burst_.store(0U, std::memory_order_release);
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

AudioStatus AudioHost::status() const noexcept {
  return AudioStatus{sample_rate_.load(std::memory_order_acquire),
                     frames_per_burst_.load(std::memory_order_acquire),
                     audio_api_.load(std::memory_order_acquire),
                     callback_count_.load(std::memory_order_relaxed),
                     rendered_frames_.load(std::memory_order_relaxed),
                     render_failures_.load(std::memory_order_relaxed),
                     non_finite_samples_.load(std::memory_order_relaxed),
                     last_error_.load(std::memory_order_acquire),
                     active_.load(std::memory_order_acquire),
                     disconnected_.load(std::memory_order_acquire)};
}

oboe::DataCallbackResult AudioHost::onAudioReady(oboe::AudioStream* stream, void* audio_data,
                                                 std::int32_t frame_count) {
  (void)stream;
  auto* output = static_cast<float*>(audio_data);
  if (output == nullptr || frame_count <= 0) {
    render_failures_.fetch_add(1U, std::memory_order_relaxed);
    return oboe::DataCallbackResult::Stop;
  }
  if (!active_.load(std::memory_order_acquire) || !runtime_ready_.load(std::memory_order_acquire)) {
    silence(output, frame_count);
    return oboe::DataCallbackResult::Continue;
  }

  const mol_result_t result =
      mol_platform_audio_render_f32(&runtime_, output, static_cast<std::uint32_t>(frame_count));
  if (result != MOL_OK) {
    silence(output, frame_count);
    render_failures_.fetch_add(1U, std::memory_order_relaxed);
  } else {
    const std::size_t sample_count = static_cast<std::size_t>(frame_count) * kChannelCount;
    for (std::size_t index = 0; index < sample_count; ++index) {
      if (!std::isfinite(output[index])) {
        output[index] = 0.0F;
        non_finite_samples_.fetch_add(1U, std::memory_order_relaxed);
      }
    }
  }
  callback_count_.fetch_add(1U, std::memory_order_relaxed);
  rendered_frames_.fetch_add(static_cast<std::uint64_t>(frame_count), std::memory_order_relaxed);
  return oboe::DataCallbackResult::Continue;
}

void AudioHost::onErrorBeforeClose(oboe::AudioStream* stream, oboe::Result error) {
  (void)stream;
  active_.store(false, std::memory_order_release);
  last_error_.store(static_cast<std::int32_t>(error), std::memory_order_release);
}

void AudioHost::onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) {
  (void)stream;
  last_error_.store(static_cast<std::int32_t>(error), std::memory_order_release);
  disconnected_.store(true, std::memory_order_release);
}

}  // namespace mol::android
