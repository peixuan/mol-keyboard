# Quality Gates

| Gate | State | Evidence |
|---|---|---|
| M0 engineering baseline | in progress | Windows x64 and Wasm Debug/Release core plus C/C++ consumers verified; ESP-IDF pending |
| M1 portable sound path | in progress | Native/Wasm synthesis and deterministic PCM16 WAV verified; realtime desktop, AudioWorklet, and ESP32 I2S pending |
| M2 music semantics | not started | Depends on M1 |
| M3 instruments and DSP | not started | Depends on M2 |
| M4 recording and tools | not started | Depends on M3 |
| M5 desktop headless product | not started | Depends on M4 |
| M6 Web/PWA | not started | Depends on portable core gates |
| M7 Android and iOS | not started | Depends on portable core gates |
| M8 HarmonyOS | not started | Depends on portable core gates |
| M9 ESP32 product | not started | Depends on portable core gates |
| M10 v1.0.0 release | not started | All earlier gates required |

The status `build-verified`, `runtime-verified`, or `device-verified` is used only
after the corresponding real check succeeds.
