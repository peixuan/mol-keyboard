// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_ANDROID_SOURCE_CHECK_JNI_H
#define MOL_ANDROID_SOURCE_CHECK_JNI_H

using jint = int;
using jlong = long long;
using jfloat = float;
using jsize = int;

struct _jobject;
struct _jlongArray;
using jobject = _jobject*;
using jlongArray = _jlongArray*;

struct JNIEnv {
  jlongArray NewLongArray(jsize length);
  void SetLongArrayRegion(jlongArray array, jsize start, jsize length, const jlong* values);
};

#define JNIEXPORT
#define JNICALL

#endif
