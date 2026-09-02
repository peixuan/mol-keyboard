# Platform Matrix

Statuses follow `CODEX_GOAL.md`. A platform is never promoted without a real
toolchain result recorded here.

| Platform | Status | Verified | Toolchain / device | Command and evidence |
|---|---|---|---|---|
| Windows x64 | planned | — | VS 2026 discovered; not yet built | Native M0 build pending |
| Windows arm64 | planned | — | Not evaluated | Pending |
| Linux x86_64 | planned | — | Not available on this host | Pending CI or Linux host |
| Linux aarch64 | planned | — | Not available on this host | Pending CI or device |
| macOS arm64/x64 | planned | — | Apple SDK not available on this host | Pending macOS host |
| WebAssembly | planned | — | Emscripten not installed | `emcc` discovery failed |
| Android arm64 | planned | — | Android NDK not discovered | Pending SDK discovery |
| iOS arm64 | planned | — | Apple SDK not available on this host | Pending macOS host/device |
| HarmonyOS arm64 | planned | — | DevEco SDK not discovered | Pending SDK and device |
| ESP32 | planned | — | ESP-IDF not installed | `idf.py` discovery failed |
| ESP32-S3 | planned | — | ESP-IDF not installed | `idf.py` discovery failed |

## Capability boundaries

- Desktop and mobile Bluetooth audio is routed by the operating system.
- Bluetooth latency does not use the wired low-latency acceptance threshold.
- Mobile hardware keyboard input is promised only while the application can
  legally receive it.
- Browser keyboard input requires focus and browser audio requires a user gesture.
- ESP32-S3 does not advertise Classic Bluetooth A2DP Source capability.
