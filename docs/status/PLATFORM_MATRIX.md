# Platform Matrix

Statuses follow `CODEX_GOAL.md`. A platform is never promoted without a real
toolchain result recorded here.

| Platform | Status | Verified | Toolchain / device | Command and evidence |
|---|---|---|---|---|
| Windows x64 | build-verified (M0 core) | 2026-09-02 | VS 2026 18.8.0; MSVC 19.51.36248; Ninja 1.13.2 | Debug/Release `mol_core`; 3/3 CTest tests passed in each build |
| Windows arm64 | planned | — | Not evaluated | Pending |
| Linux x86_64 | planned | — | Not available on this host | Pending CI or Linux host |
| Linux aarch64 | planned | — | Not available on this host | Pending CI or device |
| macOS arm64/x64 | planned | — | Apple SDK not available on this host | Pending macOS host |
| WebAssembly | runtime-verified (M1 core/worklet) | 2026-09-02 | Emscripten 6.0.5; Node.js 22.16.0; Chromium | Debug/Release 6/6 tests; real AudioWorklet C4 261.25 Hz; Release worklet 15,089 bytes |
| Android arm64 | planned | — | Android NDK not discovered | Pending SDK discovery |
| iOS arm64 | planned | — | Apple SDK not available on this host | Pending macOS host/device |
| HarmonyOS arm64 | planned | — | DevEco SDK not discovered | Pending SDK and device |
| ESP32 | build-verified (M1 core/I2S) | 2026-09-02 | ESP-IDF 6.1; GNU 15.2.0 | Configurable standard-I2S firmware built; image 124,256 bytes; mapped core flash code 2,660 bytes |
| ESP32-S3 | build-verified (M1 core/I2S) | 2026-09-02 | ESP-IDF 6.1; GNU 15.2.0 | Configurable standard-I2S firmware built; image 149,520 bytes; mapped core flash code 2,672 bytes; no Classic A2DP claim |

## Capability boundaries

- Desktop and mobile Bluetooth audio is routed by the operating system.
- Bluetooth latency does not use the wired low-latency acceptance threshold.
- Mobile hardware keyboard input is promised only while the application can
  legally receive it.
- Browser keyboard input requires focus and browser audio requires a user gesture.
- ESP32-S3 does not advertise Classic Bluetooth A2DP Source capability.
