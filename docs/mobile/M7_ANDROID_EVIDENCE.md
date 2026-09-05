# M7 Android Evidence

The Android half of M7 is implementation-complete and runtime-verified on an
Android emulator. This is not physical-device evidence and does not complete
the combined Android/iOS milestone.

## Reproducible build

The verified toolchain on 2026-09-05 was:

- Eclipse Temurin JDK 21.0.12.1;
- Gradle 8.11.1 with its distribution and wrapper JAR checksum locked;
- Android Gradle Plugin 8.10.1 and Kotlin 2.1.20;
- Android platform/API 36 revision 2 and Build Tools 36.0.0;
- Android NDK 28.2.13676358 and CMake 3.31.6;
- Oboe 1.10.0;
- Emscripten 6.0.5 and Node.js 22.16.0 for the packaged shared UI.

With `JAVA_HOME`, `ANDROID_HOME`, and `EMSDK` pointing to those tools, the
complete pipeline was run from the repository root:

```powershell
& "$env:EMSDK/emsdk_env.ps1"
platforms/android/build-app.ps1 Debug
$env:MOL_SKIP_WEB_BUILD = "1"
platforms/android/build-app.ps1 DebugAndroidTest
platforms/android/build-app.ps1 Release
Push-Location platforms/android
./gradlew.bat :app:lintDebug --no-daemon --no-configuration-cache
Pop-Location
```

An exact clean clone of `0dbce9e869458f0a7fba6e7c249c1586d6911b0a`, made
without hardlinks or copied build output, compiled all 108 Emscripten actions,
passed 46/46 MinSizeRel CTest tests, installed 20 packages from the exact npm
lock with zero vulnerabilities, passed 12/12 Web tests, type-checked and built
the production UI, and assembled the Android Debug APK. A subsequent clean
Gradle invocation assembled the instrumentation and unsigned Release APKs and
passed `lintDebug`; 114 tasks completed successfully. Debug and Release both
contain `arm64-v8a` and `x86_64` native libraries. A separate Debug build with
`-PmolApplicationId=org.example.molkeyboard` produced that exact package ID,
confirming the release identifier is configurable.

The exact clean-clone artifacts were:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 3,644,823 | `26a188c57268b4ccaa4d117bfd869befe7ecd8fb274e5278a4b799dea84dc64f` |
| `app-release-unsigned.apk` | 2,590,764 | `6010ee48225b3b050b307b3b5ab4dac07ab5e681dabf91d6632a4bba05a2e96e` |
| `app-debug-androidTest.apk` | 26,136 | `8fa70db11310716ef762d60cc41743077c5b0739a554c8bbe2d9261e16b16908` |

The unsigned Release APK targets API 36 with minimum API 26. Archive
inspection found both native ABIs, the packaged local `index.html`,
`PRIVACY.md`, `THIRD_PARTY_NOTICES.md`, and exactly one each of the paired
`generated/mol_audio_worklet_core.js` and `.wasm` assets. The manifest has no
`INTERNET` permission and declares only the notification and media-playback
foreground service permissions needed by the product.

## Emulator runtime

The packaged Debug application and its instrumentation APK were installed on
the official Android 15/API 35 Google APIs x86_64 emulator image, using emulator
37.1.11. The reported device was `sdk_gphone64_x86_64`; its audio mixer ran at
48 kHz.

The fail-closed runner performs installation, notification permission, bounded
instrumentation, result parsing, foreground-service leak detection, and cleanup:

```powershell
python tools/android_emulator_gate.py `
  --adb "$env:ANDROID_HOME/platform-tools/adb.exe" `
  --debug-apk platforms/android/app/build/outputs/apk/debug/app-debug.apk `
  --test-apk platforms/android/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk `
  --report build/android-emulator-gate.json
```

The final result was:

```text
INSTRUMENTATION_RESULT: audioApi=2
INSTRUMENTATION_RESULT: backgroundCallbacks=101
INSTRUMENTATION_RESULT: callbacks=50
INSTRUMENTATION_RESULT: focusInterrupted=true
INSTRUMENTATION_RESULT: focusResumedCallbacks=4
INSTRUMENTATION_RESULT: frames=28800
INSTRUMENTATION_RESULT: hardwareKeys=30
INSTRUMENTATION_RESULT: hardwareRepeatSuppressed=true
INSTRUMENTATION_RESULT: idleBackgroundStopped=true
INSTRUMENTATION_RESULT: lockedCallbacks=224
INSTRUMENTATION_RESULT: sampleRate=48000
INSTRUMENTATION_CODE: -1
```

`audioApi=2` is the Oboe AAudio backend. The test drives the real packaged
start button and then crosses the Web UI, strict JavaScript bridge, bound
foreground service, JNI, Oboe, and shared C engine. It sends Note On/Off and
asserts finite rendering with zero render failures. It also dispatches every
one of the 30 production Android hardware-key mappings through the real
`MainActivity.dispatchKeyEvent` path, requires the exact Note Started/Released
events from the service, and verifies that a repeated KeyDown neither retriggers
the note nor leaks ownership. Mapped keys are handled before WebView dispatch,
so WebView focus cannot consume a promised native instrument key. The
instrumentation then
injects transient focus loss into the production service listener, observes
the AAudio runtime stop, injects focus gain, and requires a newly opened stream
to resume finite callbacks. This found and fixed foreground resume eligibility;
the injection validates service recovery but does not claim Android focus
arbitration from a competing application. It then starts the core
metronome/transport, backgrounds the activity, confirms the service is a
`mediaPlayback` foreground service with its notification, and observes the
callback count advance. After turning the screen off it advances from 101 to
224 callbacks. The test wakes the emulator, disables active content,
backgrounds the app again, and verifies both the native stream and foreground
state stop instead of abusing background execution.

The checked-in Android CI job installs the official API 35 Google APIs x86_64
system image, creates and boots an AVD without a window, runs this same parser,
and kills the emulator in a bounded cleanup step. A portable source audit and
the runner's parser self-tests pass locally. The workflow itself has not run on
this unpushed commit, so only the local Windows emulator result is claimed.

## Architecture and boundaries

- Kotlin and the packaged WebView carry only bounded commands, events, status,
  and sequence bytes. PCM is generated by the C engine in the Oboe callback and
  never crosses JNI.
- The callback performs no allocation, blocking, logging, stream lifecycle, or
  JVM/JavaScript call. Stream restart and sequence parsing occur on control
  threads.
- The service requests media audio focus, handles permanent and transient focus
  loss, follows Android route notifications, debounces device restart, and
  restores persistent engine/sequence state after a restart.
- Background continuation is limited to user-started sequence playback or the
  metronome. Foreground-only live notes are released on pause. Ordinary hardware
  keyboard events are supported only while the activity is foregrounded.
- Bluetooth pairing and selection remain Android system responsibilities. The
  app follows the resulting route and does not claim an application-level
  pairing stack.

## Unclaimed acceptance

No physical Android device was available. Arm64 packaging is build-verified,
and all 30 mappings are runtime-verified with injected Android key events, but
actual arm64 playback, physical-key delivery, wired/Bluetooth route changes,
audio-focus arbitration by another application, measured end-to-end latency,
and long-duration underrun behavior remain device checks. Production signing
credentials are also intentionally absent. Android therefore has emulator
`runtime-verified` evidence, not `device-verified` or `release-ready` status.
