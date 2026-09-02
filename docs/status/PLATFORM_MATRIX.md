# Platform Matrix

Statuses follow `CODEX_GOAL.md`. A platform is never promoted without a real
toolchain result recorded here.

| Platform | Status | Verified | Toolchain / device | Command and evidence |
|---|---|---|---|---|
| Windows x64 | runtime-verified (M5 headless service) | 2026-09-03 | VS 2026 18.8.0; MSVC 19.51.36248; miniaudio 0.11.25; WASAPI; Raw Input; Clang sanitizers | Debug/Release 63/63; independent daemon/CLI over Named Pipe; real 48 kHz output and physical input adapter; ASan/UBSan 30/30 with four parser fuzzers; Bluetooth output not exposed |
| Windows arm64 | planned | — | Not evaluated | Pending |
| Linux x86_64 | runtime-verified (M5 service, WSL/null audio) | 2026-09-03 | WSL; Clang 21.1.8 | 63/63 tests; independent daemon/CLI over mode-0600 Unix socket; evdev and native audio hardware not exposed |
| Linux aarch64 | planned | — | Not available on this host | Pending CI or device |
| macOS arm64/x64 | source-reviewed (M5 IOHID/service entry) | 2026-09-03 | Apple SDK unavailable | IOHIDManager adapter and launchd asset present; actual compilation/audio/input remain unverified |
| WebAssembly | runtime-verified (M4 core/worklet) | 2026-09-03 | Emscripten 6.0.5; Node.js 22.16.0; Chromium | Debug/Release 24/24 tests; shared 12-event sequence fixture and all 18 Standard-preset PCM metrics match Native goldens; real AudioWorklet C4 261.25 Hz |
| Android arm64/x86_64 | source-checked (M1 Oboe entry) | 2026-09-02 | Oboe 1.10.0 headers; MSVC strict source check; Android NDK unavailable | Kotlin/JNI/Oboe path present; actual ABI build and device remain unverified |
| iOS arm64 | source-reviewed (M1 AudioUnit entry) | 2026-09-02 | Apple SDK unavailable on this host | Objective-C++/AVAudioSession/AudioUnit path present; compilation and device remain unverified |
| HarmonyOS arm64 | source-checked (M1 OHAudio entry) | 2026-09-02 | OpenHarmony audio declarations at c2e9f5b; MSVC strict source check; DevEco SDK unavailable | ArkTS/Node-API/OHAudio path present; OHOS build and device remain unverified |
| ESP32 | build-verified (M4 Tiny core/I2S) | 2026-09-03 | ESP-IDF 6.1; GNU 15.2.0 | Firmware parses the shared 12-event fixture before I2S startup; app binary 153,440 bytes; 33,572 bytes internal DRAM remain |
| ESP32-S3 | build-verified (M4 Tiny core/I2S) | 2026-09-03 | ESP-IDF 6.1; GNU 15.2.0 | Firmware parses the shared 12-event fixture before I2S startup; app binary 179,328 bytes; 169,105 bytes internal RAM remain; no Classic A2DP claim |

## Capability boundaries

- Desktop and mobile Bluetooth audio is routed by the operating system.
- Bluetooth latency does not use the wired low-latency acceptance threshold.
- Mobile hardware keyboard input is promised only while the application can
  legally receive it.
- Browser keyboard input requires focus and browser audio requires a user gesture.
- ESP32-S3 does not advertise Classic Bluetooth A2DP Source capability.
