// SPDX-License-Identifier: Apache-2.0
#include <jni.h>

#include <cstdint>
#include <limits>
#include <new>
#include <vector>

#include "android_audio_host.h"

namespace {

mol::android::AudioHost* from_handle(jlong handle) {
  return reinterpret_cast<mol::android::AudioHost*>(static_cast<std::intptr_t>(handle));
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeCreate(JNIEnv*, jobject) {
  auto* host = new (std::nothrow) mol::android::AudioHost();
  return static_cast<jlong>(reinterpret_cast<std::intptr_t>(host));
}

extern "C" JNIEXPORT jint JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeStart(JNIEnv*, jobject, jlong handle) {
  auto* host = from_handle(handle);
  if (host == nullptr) return static_cast<jint>(oboe::Result::ErrorNull);
  try {
    return static_cast<jint>(host->start());
  } catch (...) {
    return static_cast<jint>(oboe::Result::ErrorInternal);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeStop(JNIEnv*, jobject, jlong handle) {
  auto* host = from_handle(handle);
  if (host != nullptr) {
    host->stop();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeDestroy(JNIEnv*, jobject, jlong handle) {
  delete from_handle(handle);
}

extern "C" JNIEXPORT jint JNICALL Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeNoteOn(
    JNIEnv*, jobject, jlong handle, jint note, jfloat velocity, jlong gesture_id) {
  auto* host = from_handle(handle);
  if (host == nullptr || note < 0 || note > 127) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return host->note_on(static_cast<std::uint8_t>(note), velocity,
                       static_cast<std::uint64_t>(gesture_id));
}

extern "C" JNIEXPORT jint JNICALL Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeNoteOff(
    JNIEnv*, jobject, jlong handle, jint note, jlong gesture_id) {
  auto* host = from_handle(handle);
  if (host == nullptr || note < 0 || note > 127) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return host->note_off(static_cast<std::uint8_t>(note), static_cast<std::uint64_t>(gesture_id));
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeStatus(JNIEnv* environment, jobject,
                                                                jlong handle) {
  constexpr jsize kStatusFieldCount = 10;
  const jlong empty[kStatusFieldCount]{};
  auto* host = from_handle(handle);
  const mol::android::AudioStatus status =
      host == nullptr ? mol::android::AudioStatus{} : host->status();
  const jlong fields[kStatusFieldCount] = {
      status.sample_rate,
      status.frames_per_burst,
      status.audio_api,
      static_cast<jlong>(status.callback_count),
      static_cast<jlong>(status.rendered_frames),
      status.render_failures,
      status.non_finite_samples,
      status.last_error,
      status.active ? 1 : 0,
      status.disconnected ? 1 : 0,
  };
  jlongArray result = environment->NewLongArray(kStatusFieldCount);
  if (result != nullptr) {
    environment->SetLongArrayRegion(result, 0, kStatusFieldCount, host == nullptr ? empty : fields);
  }
  return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeSubmitControl(
    JNIEnv*, jobject, jlong handle, jint command_type, jlong gesture_id, jint integer_0,
    jint integer_1, jint integer_2, jint integer_3, jfloat scalar_0, jfloat scalar_1) {
  auto* host = from_handle(handle);
  return host == nullptr
             ? MOL_ERROR_INVALID_ARGUMENT
             : host->submit_control(static_cast<std::uint32_t>(command_type),
                                    static_cast<std::uint64_t>(gesture_id), integer_0, integer_1,
                                    integer_2, integer_3, scalar_0, scalar_1);
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativePollEvents(JNIEnv* environment, jobject,
                                                                    jlong handle) {
  constexpr std::uint32_t kMaximumEvents = 64U;
  constexpr std::uint32_t kFieldsPerEvent = 5U;
  mol_event_t events[kMaximumEvents]{};
  jlong fields[kMaximumEvents * kFieldsPerEvent]{};
  auto* host = from_handle(handle);
  const std::uint32_t count = host == nullptr ? 0U : host->poll_events(events, kMaximumEvents);
  for (std::uint32_t index = 0U; index < count; ++index) {
    const mol_event_t& event = events[index];
    const std::size_t offset = static_cast<std::size_t>(index) * kFieldsPerEvent;
    fields[offset] = event.event_type;
    fields[offset + 1U] = static_cast<jlong>(event.gesture_id);
    fields[offset + 2U] = static_cast<jlong>(event.frame);
    fields[offset + 3U] = event.payload[MOL_EVENT_PAYLOAD_NOTE];
    fields[offset + 4U] = event.payload[0];
  }
  const jsize field_count = static_cast<jsize>(count * kFieldsPerEvent);
  jlongArray result = environment->NewLongArray(field_count);
  if (result != nullptr && field_count > 0) {
    environment->SetLongArrayRegion(result, 0, field_count, fields);
  }
  return result;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeExportRecording(JNIEnv* environment,
                                                                         jobject, jlong handle) {
  auto* host = from_handle(handle);
  std::vector<std::uint8_t> bytes;
  if (host == nullptr || host->export_recording(&bytes) != MOL_OK ||
      bytes.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
    return nullptr;
  }
  jbyteArray result = environment->NewByteArray(static_cast<jsize>(bytes.size()));
  if (result != nullptr && !bytes.empty()) {
    environment->SetByteArrayRegion(result, 0, static_cast<jsize>(bytes.size()),
                                    reinterpret_cast<const jbyte*>(bytes.data()));
  }
  return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_cn_zhangpeixuan_molkeyboard_audio_NativeAudio_nativeLoadRecording(JNIEnv* environment, jobject,
                                                                       jlong handle,
                                                                       jbyteArray payload) {
  auto* host = from_handle(handle);
  if (host == nullptr || payload == nullptr) return MOL_ERROR_INVALID_ARGUMENT;
  const jsize size = environment->GetArrayLength(payload);
  if (size <= 0 || size > 2 * 1024 * 1024) return MOL_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    environment->GetByteArrayRegion(payload, 0, size, reinterpret_cast<jbyte*>(bytes.data()));
    return host->load_recording(bytes.data(), bytes.size());
  } catch (...) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
}
