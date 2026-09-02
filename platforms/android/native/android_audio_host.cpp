// SPDX-License-Identifier: Apache-2.0
#include "android_audio_host.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

namespace mol::android {
namespace {

constexpr std::uint32_t kChannelCount = 2U;
constexpr std::int32_t kBufferBurstCount = 2;
constexpr std::size_t kMaximumSequenceBytes = 2U * 1024U * 1024U;

struct SequenceWriter {
  std::vector<std::uint8_t>* bytes;
};

struct SequenceReader {
  const std::uint8_t* bytes;
  std::size_t size;
  std::size_t offset;
  mol_sequence_event_t* events;
  std::uint32_t event_capacity;
  std::uint32_t event_count;
};

mol_result_t write_sequence(void* user_data, const std::uint8_t* data, std::size_t size) {
  auto* writer = static_cast<SequenceWriter*>(user_data);
  if (writer == nullptr || writer->bytes == nullptr || data == nullptr ||
      writer->bytes->size() > kMaximumSequenceBytes ||
      size > kMaximumSequenceBytes - writer->bytes->size()) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  try {
    writer->bytes->insert(writer->bytes->end(), data, data + size);
  } catch (...) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
  return MOL_OK;
}

std::size_t read_sequence(void* user_data, std::uint8_t* data, std::size_t capacity) {
  auto* reader = static_cast<SequenceReader*>(user_data);
  if (reader == nullptr || data == nullptr || reader->offset > reader->size) return 0U;
  const std::size_t remaining = reader->size - reader->offset;
  const std::size_t copy_size = capacity < remaining ? capacity : remaining;
  std::memcpy(data, reader->bytes + reader->offset, copy_size);
  reader->offset += copy_size;
  return copy_size;
}

mol_result_t collect_sequence_event(void* user_data, const mol_sequence_event_t* event) {
  auto* reader = static_cast<SequenceReader*>(user_data);
  if (reader == nullptr || event == nullptr || reader->events == nullptr ||
      reader->event_count >= reader->event_capacity) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  reader->events[reader->event_count++] = *event;
  return MOL_OK;
}

mol_command_t make_command(std::uint32_t command_type, std::uint64_t gesture_id) {
  mol_command_t command{};
  command.struct_size = sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = command_type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = gesture_id;
  return command;
}

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
      ->setUsage(oboe::Usage::Media)
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
  if (!runtime_ready_.load(std::memory_order_acquire)) return MOL_ERROR_INVALID_STATE;
  if (note > 127U || !std::isfinite(velocity) || velocity < 0.0F || velocity > 1.0F) {
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

mol_result_t AudioHost::submit_control(std::uint32_t command_type, std::uint64_t gesture_id,
                                       std::int32_t integer_0, std::int32_t integer_1,
                                       std::int32_t integer_2, std::int32_t integer_3,
                                       float scalar_0, float scalar_1) {
  if (!runtime_ready_.load(std::memory_order_acquire)) return MOL_ERROR_INVALID_STATE;
  if (!std::isfinite(scalar_0) || !std::isfinite(scalar_1)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  mol_command_t command = make_command(command_type, gesture_id);
  switch (command_type) {
    case MOL_COMMAND_NOTE_ON:
      if (integer_0 < 0 || integer_0 > 127 || scalar_0 < 0.0F || scalar_0 > 1.0F) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.note.note = static_cast<std::uint8_t>(integer_0);
      command.payload.note.velocity = scalar_0;
      break;
    case MOL_COMMAND_NOTE_OFF:
      if (integer_0 < 0 || integer_0 > 127) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.note.note = static_cast<std::uint8_t>(integer_0);
      break;
    case MOL_COMMAND_SUSTAIN:
    case MOL_COMMAND_SET_MASTER_GAIN:
    case MOL_COMMAND_SET_TEMPO:
      command.payload.scalar.value = scalar_0;
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
    case MOL_COMMAND_SET_TRANSPOSE:
    case MOL_COMMAND_SET_CHORD_MODE:
      command.payload.integer.value = integer_0;
      break;
    case MOL_COMMAND_SET_PRESET:
      if (integer_0 < 0) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.preset.preset = static_cast<std::uint32_t>(integer_0);
      command.payload.preset.hard_switch = integer_1 != 0 ? 1U : 0U;
      break;
    case MOL_COMMAND_SET_PARAMETER:
      if (integer_0 < 0) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.parameter.parameter = static_cast<std::uint32_t>(integer_0);
      command.payload.parameter.value = scalar_0;
      break;
    case MOL_COMMAND_SET_SCALE:
      if (integer_0 < 0 || integer_1 < 0 || integer_1 > 11 || integer_2 < 0 ||
          integer_2 > std::numeric_limits<std::uint8_t>::max()) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.scale.type = static_cast<std::uint32_t>(integer_0);
      command.payload.scale.tonic = static_cast<std::uint8_t>(integer_1);
      command.payload.scale.mapping = static_cast<std::uint8_t>(integer_2);
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      if (integer_0 < 0 || integer_1 < 0 || integer_2 < 0 ||
          integer_2 > std::numeric_limits<std::uint8_t>::max() || scalar_0 < 0.0F) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.arpeggiator.mode = static_cast<std::uint32_t>(integer_0);
      command.payload.arpeggiator.rate = static_cast<std::uint32_t>(integer_1);
      command.payload.arpeggiator.octaves = static_cast<std::uint8_t>(integer_2);
      command.payload.arpeggiator.random_seed = static_cast<std::uint32_t>(integer_3);
      command.payload.arpeggiator.gate = scalar_0;
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      if (integer_0 <= 0 || integer_0 > std::numeric_limits<std::uint8_t>::max() ||
          integer_1 <= 0 || integer_1 > std::numeric_limits<std::uint8_t>::max()) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      command.payload.time_signature.numerator = static_cast<std::uint8_t>(integer_0);
      command.payload.time_signature.denominator = static_cast<std::uint8_t>(integer_1);
      break;
    case MOL_COMMAND_SET_METRONOME:
      command.payload.metronome.enabled = integer_0 != 0 ? 1U : 0U;
      command.payload.metronome.level = scalar_0;
      break;
    case MOL_COMMAND_SET_PORTAMENTO:
      if (integer_0 < 0 || scalar_0 < 0.0F) return MOL_ERROR_INVALID_ARGUMENT;
      command.payload.portamento.mode = static_cast<std::uint32_t>(integer_0);
      command.payload.portamento.time_ms = scalar_0;
      break;
    case MOL_COMMAND_ALL_NOTES_OFF:
    case MOL_COMMAND_ALL_SOUND_OFF:
    case MOL_COMMAND_TRANSPORT_START:
    case MOL_COMMAND_TRANSPORT_STOP:
    case MOL_COMMAND_RECORD_START:
    case MOL_COMMAND_RECORD_STOP:
    case MOL_COMMAND_PLAYBACK_START:
    case MOL_COMMAND_PLAYBACK_STOP:
    case MOL_COMMAND_RESET_ENGINE:
      break;
    default:
      return MOL_ERROR_INVALID_ARGUMENT;
  }
  return mol_platform_audio_submit(&runtime_, &command);
}

std::uint32_t AudioHost::poll_events(mol_event_t* events, std::uint32_t capacity) noexcept {
  if (!runtime_ready_.load(std::memory_order_acquire) || runtime_.engine == nullptr ||
      events == nullptr || capacity == 0U) {
    return 0U;
  }
  return mol_engine_poll_events(runtime_.engine, events, capacity);
}

mol_result_t AudioHost::export_recording(std::vector<std::uint8_t>* output) {
  if (!runtime_ready_.load(std::memory_order_acquire) || runtime_.engine == nullptr ||
      output == nullptr) {
    return MOL_ERROR_INVALID_STATE;
  }
  try {
    std::vector<mol_sequence_event_t> events(MOL_PROFILE_SEQUENCE_EVENTS);
    mol_sequence_config_t config{};
    config.struct_size = sizeof(config);
    config.api_version = MOL_API_VERSION;
    std::uint32_t event_count = 0U;
    mol_result_t result =
        mol_engine_copy_recording(runtime_.engine, &config, events.data(),
                                  static_cast<std::uint32_t>(events.size()), &event_count);
    if (result != MOL_OK || event_count == 0U) {
      return result == MOL_OK ? MOL_ERROR_INVALID_STATE : result;
    }
    output->clear();
    output->reserve(4096U);
    SequenceWriter destination{output};
    mol_sequence_writer_t writer{};
    writer.struct_size = sizeof(writer);
    writer.api_version = MOL_API_VERSION;
    result = mol_sequence_writer_init(&writer, &config, write_sequence, &destination);
    for (std::uint32_t index = 0U; result == MOL_OK && index < event_count; ++index) {
      result = mol_sequence_writer_append(&writer, &events[index]);
    }
    if (result == MOL_OK) result = mol_sequence_writer_finalize(&writer);
    if (result != MOL_OK) output->clear();
    return result;
  } catch (...) {
    output->clear();
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
}

mol_result_t AudioHost::load_recording(const std::uint8_t* data, std::size_t size) {
  if (!runtime_ready_.load(std::memory_order_acquire) || runtime_.engine == nullptr ||
      data == nullptr || size == 0U || size > kMaximumSequenceBytes) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::vector<mol_sequence_event_t> events(MOL_PROFILE_SEQUENCE_EVENTS);
    SequenceReader source{data, size, 0U, events.data(), static_cast<std::uint32_t>(events.size()),
                          0U};
    mol_sequence_callbacks_t callbacks{};
    callbacks.struct_size = sizeof(callbacks);
    callbacks.api_version = MOL_API_VERSION;
    callbacks.on_event = collect_sequence_event;
    callbacks.user_data = &source;
    mol_sequence_config_t config{};
    config.struct_size = sizeof(config);
    config.api_version = MOL_API_VERSION;
    const mol_result_t result =
        mol_sequence_read_stream(read_sequence, &source, &config, &callbacks);
    if (result != MOL_OK || source.offset != source.size) return result;
    return load_recording_while_paused(&config, events.data(), source.event_count);
  } catch (...) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
}

mol_result_t AudioHost::load_recording_while_paused(const mol_sequence_config_t* config,
                                                    const mol_sequence_event_t* events,
                                                    std::uint32_t event_count) {
  if (stream_ == nullptr || runtime_.engine == nullptr) return MOL_ERROR_INVALID_STATE;
  const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
  while (callbacks_in_flight_.load(std::memory_order_acquire) != 0U &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  if (callbacks_in_flight_.load(std::memory_order_acquire) != 0U) {
    active_.store(was_active, std::memory_order_release);
    return MOL_ERROR_INVALID_STATE;
  }
  const mol_result_t result =
      mol_engine_load_sequence(runtime_.engine, config, events, event_count);
  active_.store(was_active, std::memory_order_release);
  return result;
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
  callbacks_in_flight_.fetch_add(1U, std::memory_order_acq_rel);
  auto* output = static_cast<float*>(audio_data);
  if (output == nullptr || frame_count <= 0) {
    render_failures_.fetch_add(1U, std::memory_order_relaxed);
    callbacks_in_flight_.fetch_sub(1U, std::memory_order_release);
    return oboe::DataCallbackResult::Stop;
  }
  if (!active_.load(std::memory_order_acquire) || !runtime_ready_.load(std::memory_order_acquire)) {
    silence(output, frame_count);
    callbacks_in_flight_.fetch_sub(1U, std::memory_order_release);
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
  callbacks_in_flight_.fetch_sub(1U, std::memory_order_release);
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
