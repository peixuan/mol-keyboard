// SPDX-License-Identifier: Apache-2.0
#include <jni.h>

#include <cstdint>
#include <new>

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
  return host == nullptr ? static_cast<jint>(oboe::Result::ErrorNull)
                         : static_cast<jint>(host->start());
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
