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

## Application runtime

The Gradle application under `platforms/android/app` packages the complete
local Web UI and connects it to the native runtime through a strict,
versioned, allow-listed bridge. It does not request `INTERNET`, disables backup,
blocks remote navigation, and exposes no arbitrary file or command interface.

The bound `mediaPlayback` foreground service owns `AudioRuntime` independently
of the Activity. It requests media audio focus, follows device callbacks and
the becoming-noisy broadcast, restarts Oboe away from its realtime callbacks,
and restores the loaded sequence and persistent controls. Only explicit
playback or metronome activity may continue after the UI is backgrounded;
otherwise the stream and foreground state stop. The normal hardware-key map is
handled by the foreground Activity and is never advertised as background input.

## Verification boundary

Debug, unsigned Release, and instrumentation APKs have been built with Android
API 36, NDK 28.2.13676358, CMake 3.31.6, and both `arm64-v8a`/`x86_64` ABIs.
Android lint passes. The packaged x86_64 application is runtime-verified on an
official Android 15/API 35 emulator through UI, bridge, JNI, AAudio, and the C
engine. The same automated run verifies foreground notification state,
background rendering, screen-off continuation, and idle shutdown. See
`docs/mobile/M7_ANDROID_EVIDENCE.md` for exact commands, hashes, and counters.

No physical device was available. Arm64 playback, physical keyboard input,
wired/Bluetooth route changes, external audio-focus interruption, latency, and
long-duration behavior therefore remain unclaimed device gates.
