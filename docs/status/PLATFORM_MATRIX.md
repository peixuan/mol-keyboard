# Platform Matrix

Statuses follow `CODEX_GOAL.md`. A platform is never promoted without a real
toolchain result recorded here.

| Platform | Status | Verified | Toolchain / device | Command and evidence |
|---|---|---|---|---|
| Windows x64 | runtime-verified (M5 headless service) | 2026-09-03 | VS 2026 18.8.0; MSVC 19.51.36248; miniaudio 0.11.25; WASAPI; Raw Input; Clang sanitizers | Debug/LTO Release 71/71; independent daemon/CLI over Named Pipe; real 48 kHz output and physical input adapter; ASan/UBSan 40/40 with six parser fuzzers; Bluetooth output not exposed |
| Windows arm64 | planned | — | Not evaluated | Pending |
| Linux x86_64 | runtime-verified (M5 service, WSL/null audio) | 2026-09-03 | WSL; Clang 21.1.8 | 63/63 tests; independent daemon/CLI over mode-0600 Unix socket; evdev and native audio hardware not exposed |
| Linux aarch64 | planned | — | Not available on this host | Pending CI or device |
| macOS arm64/x64 | source-reviewed (M5 IOHID/service entry) | 2026-09-03 | Apple SDK unavailable | IOHIDManager adapter and launchd asset present; actual compilation/audio/input remain unverified |
| WebAssembly | runtime-verified (M6 PWA; Safari pending) | 2026-09-03 | Emscripten 6.0.5; Node.js 22.16.0; Chrome 151.0.7922.175; Edge 152.0.4191.53; Firefox 153.0; Chromium 151.0.7922.34; WebKit 26.5 | Wasm Debug/Release 31/31; browser matrix 15 applicable passes and 27 explicit project skips across 42 combinations; Chrome/Edge/Chromium-mobile realtime AudioWorklet, Firefox offline AudioWorklet/Wasm DSP, Chrome offline audio startup/play, busy-main-thread stability, blur release, lifecycle resume, SAB, and MessagePort paths pass; WebKit UI/layout passes but Windows WebKit lacks AudioWorklet and is not claimed as Safari |
| Android arm64/x86_64 | runtime-verified (x86_64 emulator); arm64 build-verified | 2026-09-03 | JDK 21.0.12.1; Gradle 8.11.1; AGP 8.10.1; Kotlin 2.1.20; API 36; NDK 28.2.13676358; CMake 3.31.6; Oboe 1.10.0; Android 15/API 35 emulator | Debug/Release/test dual-ABI APKs and lint pass; packaged UI-to-AAudio chain renders at 48 kHz; foreground/background/screen-off/idle lifecycle passes; physical device and route/focus acceptance remain unverified |
| iOS arm64 | implementation-complete; source-reviewed | 2026-09-03 | Apple SDK unavailable on this host | Complete UIKit/WKWebView application, strict native bridge, RemoteIO/AVAudioSession lifecycle, foreground hardware keys, legal background policy, private sequence storage, privacy manifest, app icon, and simulator/device Xcode pipeline are present; compilation and device acceptance remain unverified |
| HarmonyOS arm64/x86_64 | implementation-complete; source-checked | 2026-09-03 | MSVC 19.51 declaration-boundary compile and 65/65 CTest; DevEco SDK unavailable | Complete Stage/ArkUI application, strict Node-API/OHAudio host, actual latency status/fallback, AudioSession/AVSession/official continuous task, private storage, and audited HAP pipeline are present; HAP build and physical-device acceptance remain unverified |
| ESP32 | build-verified (M9 implementation) | 2026-09-03 | ESP-IDF 6.1; GNU 15.2.0; no board attached | Default 1,018,096-byte and Web 1,550,992-byte images build; GPIO, BLE/Classic HID, I2S, A2DP Source/AVRCP, NVS/FAT, recovery gestures, control task, and physical AP-only configuration are implemented; `libmol_core.a` is 26,790 bytes in the default map; physical radio/audio/long-run acceptance is unverified |
| ESP32-S3 | build-verified (M9 implementation) | 2026-09-03 | ESP-IDF 6.1; GNU 15.2.0; no board attached | Default 796,656-byte and Web 1,302,048-byte images build; GPIO, BLE and USB HID, I2S, NVS/FAT, recovery gestures, control task, and physical AP-only configuration are implemented; Classic A2DP is explicitly absent; physical input/audio/long-run acceptance is unverified |

## Capability boundaries

- Desktop and mobile Bluetooth audio is routed by the operating system.
- Bluetooth latency does not use the wired low-latency acceptance threshold.
- Mobile hardware keyboard input is promised only while the application can
  legally receive it.
- Browser keyboard input requires focus and browser audio requires a user gesture.
- ESP32-S3 does not advertise Classic Bluetooth A2DP Source capability.
