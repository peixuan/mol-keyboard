# Platform Matrix

Statuses follow `CODEX_GOAL.md`. A platform is never promoted without a real
toolchain result recorded here.

| Platform | Status | Verified | Toolchain / device | Command and evidence |
|---|---|---|---|---|
| Windows x64 | runtime-verified (M1 desktop audio) | 2026-09-02 | VS 2026 18.8.0; MSVC 19.51.36248; miniaudio 0.11.25; WASAPI | Debug/Release 10/10 tests; real default device at 48 kHz; C4 261.25 Hz; zero render/non-finite failures |
| Windows arm64 | planned | — | Not evaluated | Pending |
| Linux x86_64 | planned | — | Not available on this host | Pending CI or Linux host |
| Linux aarch64 | planned | — | Not available on this host | Pending CI or device |
| macOS arm64/x64 | planned | — | Apple SDK not available on this host | Pending macOS host |
| WebAssembly | runtime-verified (M1 core/worklet) | 2026-09-02 | Emscripten 6.0.5; Node.js 22.16.0; Chromium | Debug/Release 8/8 tests; real AudioWorklet C4 261.25 Hz; Release worklet 15,089 bytes |
| Android arm64/x86_64 | source-checked (M1 Oboe entry) | 2026-09-02 | Oboe 1.10.0 headers; MSVC strict source check; Android NDK unavailable | Kotlin/JNI/Oboe path present; actual ABI build and device remain unverified |
| iOS arm64 | source-reviewed (M1 AudioUnit entry) | 2026-09-02 | Apple SDK unavailable on this host | Objective-C++/AVAudioSession/AudioUnit path present; compilation and device remain unverified |
| HarmonyOS arm64 | source-checked (M1 OHAudio entry) | 2026-09-02 | OpenHarmony audio declarations at c2e9f5b; MSVC strict source check; DevEco SDK unavailable | ArkTS/Node-API/OHAudio path present; OHOS build and device remain unverified |
| ESP32 | build-verified (M1 core/I2S) | 2026-09-02 | ESP-IDF 6.1; GNU 15.2.0 | Configurable standard-I2S firmware built; image 124,256 bytes; mapped core flash code 2,660 bytes |
| ESP32-S3 | build-verified (M1 core/I2S) | 2026-09-02 | ESP-IDF 6.1; GNU 15.2.0 | Configurable standard-I2S firmware built; image 149,520 bytes; mapped core flash code 2,672 bytes; no Classic A2DP claim |

## Capability boundaries

- Desktop and mobile Bluetooth audio is routed by the operating system.
- Bluetooth latency does not use the wired low-latency acceptance threshold.
- Mobile hardware keyboard input is promised only while the application can
  legally receive it.
- Browser keyboard input requires focus and browser audio requires a user gesture.
- ESP32-S3 does not advertise Classic Bluetooth A2DP Source capability.
