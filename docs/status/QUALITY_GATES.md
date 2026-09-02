# Quality Gates

| Gate | State | Evidence |
|---|---|---|
| M0 engineering baseline | passed | Windows x64 and Wasm Debug/Release tests pass; C/C++ consumers link; ESP32 and ESP32-S3 firmware compile the same ISO C11 core with ESP-IDF 6.1 |
| M1 portable sound path | in progress | Native/Wasm synthesis, deterministic PCM16 WAV, real Windows WASAPI and browser AudioWorklet runtime paths, and ESP32/ESP32-S3 I2S firmware builds are verified; mobile platform call entries pending |
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
