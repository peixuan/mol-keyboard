# Android Native Audio Entry

The Android M1 entry is a real Kotlin-to-JNI-to-Oboe call path. The Kotlin
`AudioRuntime` owns one native `AudioHost`; JNI only transfers lifecycle,
bounded note commands, and status. PCM never crosses JNI.

The native host opens Oboe in low-latency mode, requests exclusive output and
retries shared output when exclusive access is unavailable. Oboe selects
AAudio where supported and retains its compatibility fallback. The actual
sample rate and burst size initialize the same caller-owned `mol_core` runtime
used by the other platforms. The Oboe callback renders stereo float PCM
directly, accepts variable frame counts, replaces non-finite values with
silence, and performs no allocation, locking, logging, file access, JNI call,
or stream lifecycle operation.

Disconnection is recorded by the error callback. It does not restart or close
the stream from the realtime callback; Kotlin can observe `disconnected` and
request a new stream from its lifecycle or service control thread.

## Build entry

Set `ANDROID_NDK_HOME` to a real Android NDK and build either required ABI:

```powershell
cmake --preset android-arm64
cmake --build --preset android-arm64

cmake --preset android-x86_64
cmake --build --preset android-x86_64
```

The output library is `libmol_android_audio.so`. Copy
`platforms/android/kotlin/cn/zhangpeixuan/molkeyboard/audio/AudioRuntime.kt`
into the application module with the same package path. The package path is a
stable JNI implementation detail and does not prevent a Gradle application ID
override.

When `MOL_VALIDATE_MOBILE_SOURCES=ON` (enabled by the developer and CI presets),
CMake downloads the same checksum-locked Oboe headers and compiles both native
sources against strict warnings on non-cross-compiling builds.
The tiny `source_check/include` files only supply declarations missing from
Windows while parsing Android's public headers; they are never visible to an
Android build. This is source-level evidence, not Android build verification.

## Current verification boundary

The Android host has not been built with an NDK or run on a device on the
current Windows machine. No Android SDK or NDK was installed, and the official
NDK download endpoint was unreachable from this host. Therefore Android stays
`source-checked`, not `build-verified` or `device-verified`. Audio focus, the
`mediaPlayback` foreground service, persistent notification, UI, packaging,
and device route/background tests remain M7 work.
