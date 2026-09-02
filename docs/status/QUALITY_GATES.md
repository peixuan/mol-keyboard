# Quality Gates

| Gate | State | Evidence |
|---|---|---|
| M0 engineering baseline | passed | Windows x64 and Wasm Debug/Release tests pass; C/C++ consumers link; ESP32 and ESP32-S3 firmware compile the same ISO C11 core with ESP-IDF 6.1 |
| M1 portable sound path | passed | Same C4 sequence is 261.25 Hz with finite output in Native, Wasm, and the ESP32 32 kHz core path; allocation-free desktop/worklet/I2S callbacks and Android Oboe, Apple AudioUnit, and Harmony OHAudio call entries exist |
| M2 music semantics | passed | All required transforms, gesture ownership, sustain, portamento, transport, metronome, and six arpeggiator modes pass 17 Native and 15 Wasm tests; both targets match the 35-event golden digest; exact transport math reaches the two-hour frame with zero drift |
| M3 instruments and DSP | in progress | Highest unmet gate after M2 |
| M4 recording and tools | not started | Depends on M3 |
| M5 desktop headless product | not started | Depends on M4 |
| M6 Web/PWA | not started | Depends on portable core gates |
| M7 Android and iOS | not started | Depends on portable core gates |
| M8 HarmonyOS | not started | Depends on portable core gates |
| M9 ESP32 product | not started | Depends on portable core gates |
| M10 v1.0.0 release | not started | All earlier gates required |

The status `build-verified`, `runtime-verified`, or `device-verified` is used only
after the corresponding real check succeeds.
