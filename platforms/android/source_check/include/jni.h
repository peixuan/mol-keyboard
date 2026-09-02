// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_ANDROID_SOURCE_CHECK_JNI_H
#define MOL_ANDROID_SOURCE_CHECK_JNI_H

using jint = int;
using jlong = long long;
using jfloat = float;
using jbyte = signed char;
using jsize = int;

struct _jobject;
struct _jlongArray;
struct _jbyteArray;
using jobject = _jobject*;
using jlongArray = _jlongArray*;
using jbyteArray = _jbyteArray*;

struct JNIEnv {
  jlongArray NewLongArray(jsize length);
  void SetLongArrayRegion(jlongArray array, jsize start, jsize length, const jlong* values);
  jbyteArray NewByteArray(jsize length);
  void SetByteArrayRegion(jbyteArray array, jsize start, jsize length, const jbyte* values);
  jsize GetArrayLength(jbyteArray array);
  void GetByteArrayRegion(jbyteArray array, jsize start, jsize length, jbyte* values);
};

#define JNIEXPORT
#define JNICALL

#endif
