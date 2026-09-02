# Quality Gates

| Gate | State | Evidence |
|---|---|---|
| M0 engineering baseline | passed | Windows x64 and Wasm Debug/Release tests pass; C/C++ consumers link; ESP32 and ESP32-S3 firmware compile the same ISO C11 core with ESP-IDF 6.1 |
| M1 portable sound path | passed | Same C4 sequence is 261.25 Hz with finite output in Native, Wasm, and the ESP32 32 kHz core path; allocation-free desktop/worklet/I2S callbacks and Android Oboe, Apple AudioUnit, and Harmony OHAudio call entries exist |
| M2 music semantics | passed | All required transforms, gesture ownership, sustain, portamento, transport, metronome, and six arpeggiator modes pass 17 Native and 15 Wasm tests; both targets match the 35-event golden digest; exact transport math reaches the two-hour frame with zero drift |
| M3 instruments and DSP | passed | All DSP primitives, 18 compiled procedural patches, stereo effects/master safety, calibrated audio golden, Native/Wasm metric conformance, sanitizer fuzzing, Tiny/feature-off builds, and current ESP32/ESP32-S3 map evidence pass |
| M4 recording and tools | passed | Bounded streaming Mol Sequence v1, deterministic processed-event recording/playback, JSON/MIDI/editing tools, three-format offline WAV renderer, corruption/round-trip tests, parser/writer fuzzing, and one shared Native/Wasm/ESP32 sequence fixture pass |
| M5 desktop headless product | implementation complete; platform acceptance pending | Windows runtime and Linux/WSL lifecycle verified; physical Bluetooth/evdev and macOS remain explicit device/toolchain gates |
| M6 Web/PWA | implementation complete; Safari acceptance pending | Production PWA, standalone and authenticated service modes, Chrome/Edge/Firefox AudioWorklet automation, mobile-layout automation, offline lifecycle, MessagePort fallback, and SAB path pass; current-stable Safari requires an Apple host |
| M7 Android and iOS | in progress | Highest locally actionable gate after the M6 implementation |
| M8 HarmonyOS | not started | Depends on portable core gates |
| M9 ESP32 product | not started | Depends on portable core gates |
| M10 v1.0.0 release | not started | All earlier gates required |

The status `build-verified`, `runtime-verified`, or `device-verified` is used only
after the corresponding real check succeeds.
