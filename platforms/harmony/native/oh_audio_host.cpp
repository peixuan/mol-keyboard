// SPDX-License-Identifier: Apache-2.0
#include "oh_audio_host.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <thread>

namespace mol::harmony {
namespace {

constexpr std::int32_t kRequestedSampleRate = 48000;
constexpr std::int32_t kChannelCount = 2;
constexpr std::int32_t kBytesPerFrame = kChannelCount * static_cast<std::int32_t>(sizeof(float));
constexpr std::size_t kMaximumSequenceBytes = std::size_t{2U} * 1024U * 1024U;

struct SequenceWriter {
  std::uint8_t* bytes;
  std::size_t capacity;
  std::size_t size;
};

struct SequenceReader {
  const std::uint8_t* bytes;
  std::size_t size;
  std::size_t offset;
  mol_sequence_event_t* events;
  std::uint32_t event_capacity;
  std::uint32_t event_count;
};

void record_result(std::atomic<std::int32_t>& destination, OH_AudioStream_Result result) {
  destination.store(static_cast<std::int32_t>(result), std::memory_order_release);
}

mol_result_t write_sequence(void* user_data, const std::uint8_t* data, std::size_t size) {
  auto* writer = static_cast<SequenceWriter*>(user_data);
  if (writer == nullptr || writer->bytes == nullptr || data == nullptr ||
      writer->size > writer->capacity || size > writer->capacity - writer->size) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  std::memcpy(writer->bytes + writer->size, data, size);
  writer->size += size;
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

}  // namespace

OH_AudioData_Callback_Result write_data_callback(OH_AudioRenderer* renderer, void* user_data,
                                                 void* audio_data, std::int32_t audio_data_size) {
  (void)renderer;
  auto* host = static_cast<AudioHost*>(user_data);
  if (host == nullptr || audio_data == nullptr || audio_data_size <= 0) {
    return AUDIO_DATA_CALLBACK_RESULT_INVALID;
  }
  host->callbacks_in_flight_.fetch_add(1U, std::memory_order_acq_rel);
  if (!host->active_.load(std::memory_order_acquire) ||
      !host->runtime_ready_.load(std::memory_order_acquire)) {
    std::memset(audio_data, 0, static_cast<std::size_t>(audio_data_size));
    host->callbacks_in_flight_.fetch_sub(1U, std::memory_order_release);
    return AUDIO_DATA_CALLBACK_RESULT_VALID;
  }
  if (audio_data_size % kBytesPerFrame != 0) {
    std::memset(audio_data, 0, static_cast<std::size_t>(audio_data_size));
    host->render_failures_.fetch_add(1U, std::memory_order_relaxed);
    host->callbacks_in_flight_.fetch_sub(1U, std::memory_order_release);
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
  host->callbacks_in_flight_.fetch_sub(1U, std::memory_order_release);
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

namespace {

OH_AudioStream_Result create_renderer(AudioHost* host, OH_AudioStream_LatencyMode latency_mode,
                                      OH_AudioRenderer** renderer) {
  *renderer = nullptr;
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
    result = OH_AudioStreamBuilder_SetLatencyMode(builder, latency_mode);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MUSIC);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result =
        OH_AudioStreamBuilder_SetRendererInterruptMode(builder, AUDIOSTREAM_INTERRUPT_MODE_SHARE);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, write_data_callback, host);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererOutputDeviceChangeCallback(
        builder, output_device_change_callback, host);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererInterruptCallback(builder, interrupt_callback, host);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_SetRendererErrorCallback(builder, error_callback, host);
  }
  if (result == AUDIOSTREAM_SUCCESS) {
    result = OH_AudioStreamBuilder_GenerateRenderer(builder, renderer);
  }
  if (builder != nullptr) {
    const OH_AudioStream_Result destroy_result = OH_AudioStreamBuilder_Destroy(builder);
    if (result == AUDIOSTREAM_SUCCESS && destroy_result != AUDIOSTREAM_SUCCESS) {
      result = destroy_result;
    }
  }
  if (result != AUDIOSTREAM_SUCCESS && *renderer != nullptr) {
    (void)OH_AudioRenderer_Release(*renderer);
    *renderer = nullptr;
  }
  return result;
}

}  // namespace

AudioHost::~AudioHost() { stop(); }

OH_AudioStream_Result AudioHost::start() {
  stop();
  needs_restart_.store(false, std::memory_order_release);
  latency_fallback_used_.store(false, std::memory_order_release);
  OH_AudioStream_Result result = create_renderer(this, AUDIOSTREAM_LATENCY_MODE_FAST, &renderer_);
  if (result != AUDIOSTREAM_SUCCESS) {
    latency_fallback_used_.store(true, std::memory_order_release);
    result = create_renderer(this, AUDIOSTREAM_LATENCY_MODE_NORMAL, &renderer_);
  }
  if (result != AUDIOSTREAM_SUCCESS || renderer_ == nullptr) {
    record_result(last_error_, result);
    stop();
    needs_restart_.store(true, std::memory_order_release);
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
    needs_restart_.store(true, std::memory_order_release);
    return result;
  }

  const mol_result_t engine_result =
      mol_platform_audio_init(&runtime_, static_cast<std::uint32_t>(sample_rate),
                              static_cast<std::uint32_t>(channel_count));
  if (engine_result != MOL_OK) {
    record_result(last_error_, AUDIOSTREAM_ERROR_SYSTEM);
    stop();
    needs_restart_.store(true, std::memory_order_release);
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
    needs_restart_.store(true, std::memory_order_release);
    return result;
  }
  needs_restart_.store(false, std::memory_order_release);
  record_result(last_error_, AUDIOSTREAM_SUCCESS);
  return AUDIOSTREAM_SUCCESS;
}

void AudioHost::stop() {
  active_.store(false, std::memory_order_release);
  needs_restart_.store(false, std::memory_order_release);
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

mol_result_t AudioHost::submit_control(std::uint32_t command_type, std::uint64_t gesture_id,
                                       std::int32_t integer_0, std::int32_t integer_1,
                                       std::int32_t integer_2, std::int32_t integer_3,
                                       float scalar_0, float scalar_1) {
  if (!runtime_ready_.load(std::memory_order_acquire) || !std::isfinite(scalar_0) ||
      !std::isfinite(scalar_1)) {
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

std::uint32_t AudioHost::poll_events(mol_event_t* events, std::uint32_t capacity) {
  if (!runtime_ready_.load(std::memory_order_acquire) || runtime_.engine == nullptr ||
      events == nullptr || capacity == 0U) {
    return 0U;
  }
  return mol_engine_poll_events(runtime_.engine, events, capacity);
}

mol_result_t AudioHost::export_recording(std::uint8_t* bytes, std::size_t capacity,
                                         std::size_t* size) {
  if (!runtime_ready_.load(std::memory_order_acquire) || runtime_.engine == nullptr ||
      bytes == nullptr || size == nullptr || capacity == 0U || capacity > kMaximumSequenceBytes) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *size = 0U;
  const std::unique_ptr<mol_sequence_event_t[]> events{
      new (std::nothrow) mol_sequence_event_t[MOL_PROFILE_SEQUENCE_EVENTS]};
  if (!events) return MOL_ERROR_INSUFFICIENT_MEMORY;
  mol_sequence_config_t config{};
  config.struct_size = sizeof(config);
  config.api_version = MOL_API_VERSION;
  std::uint32_t event_count = 0U;
  mol_result_t result = mol_engine_copy_recording(runtime_.engine, &config, events.get(),
                                                  MOL_PROFILE_SEQUENCE_EVENTS, &event_count);
  if (result != MOL_OK) return result;
  if (event_count == 0U) return MOL_ERROR_INVALID_STATE;

  SequenceWriter destination{bytes, capacity, 0U};
  mol_sequence_writer_t writer{};
  writer.struct_size = sizeof(writer);
  writer.api_version = MOL_API_VERSION;
  result = mol_sequence_writer_init(&writer, &config, write_sequence, &destination);
  for (std::uint32_t index = 0U; result == MOL_OK && index < event_count; ++index) {
    result = mol_sequence_writer_append(&writer, &events[index]);
  }
  if (result == MOL_OK) result = mol_sequence_writer_finalize(&writer);
  if (result == MOL_OK) *size = destination.size;
  return result;
}

mol_result_t AudioHost::load_recording(const std::uint8_t* bytes, std::size_t size) {
  if (!runtime_ready_.load(std::memory_order_acquire) || runtime_.engine == nullptr ||
      bytes == nullptr || size == 0U || size > kMaximumSequenceBytes) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  const std::unique_ptr<mol_sequence_event_t[]> events{
      new (std::nothrow) mol_sequence_event_t[MOL_PROFILE_SEQUENCE_EVENTS]};
  if (!events) return MOL_ERROR_INSUFFICIENT_MEMORY;
  SequenceReader source{bytes, size, 0U, events.get(), MOL_PROFILE_SEQUENCE_EVENTS, 0U};
  mol_sequence_callbacks_t callbacks{};
  callbacks.struct_size = sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = collect_sequence_event;
  callbacks.user_data = &source;
  mol_sequence_config_t config{};
  config.struct_size = sizeof(config);
  config.api_version = MOL_API_VERSION;
  const mol_result_t read_result =
      mol_sequence_read_stream(read_sequence, &source, &config, &callbacks);
  if (read_result != MOL_OK) return read_result;
  if (source.offset != source.size) return MOL_ERROR_CORRUPT_DATA;

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
  const mol_result_t load_result =
      mol_engine_load_sequence(runtime_.engine, &config, events.get(), source.event_count);
  active_.store(was_active, std::memory_order_release);
  return load_result;
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
                     needs_restart_.load(std::memory_order_acquire),
                     latency_mode_.load(std::memory_order_acquire) == AUDIOSTREAM_LATENCY_MODE_FAST,
                     latency_fallback_used_.load(std::memory_order_acquire)};
}

}  // namespace mol::harmony
