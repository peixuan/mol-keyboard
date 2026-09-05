# Definition of Done Audit

This is the requirement-by-requirement release decision for section 30 of
`CODEX_GOAL.md`. It does not replace the goal or lower any acceptance boundary.
The exact executable source candidate is clean commit `19735a9`; subsequent
commits through the current audit update evidence only. Evidence is classified
as follows:

- **verified**: the requirement has direct current build or runtime evidence;
- **implementation verified; external acceptance open**: the production path
  exists and the strongest device-free gate passes, but required native-host or
  physical-device evidence is absent;
- **open**: the requirement itself is not yet satisfied and release is blocked.

| # | Requirement | State | Authoritative evidence and remaining boundary |
|---:|---|---|---|
| 1 | New, structured Git repository | verified | Repository history, the clean-room declaration in [`README.md`](../../README.md), and the current clean-checkout reproductions recorded in [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md) establish the from-scratch project and build structure. |
| 2 | Platform-independent ISO C11 `mol_core` with caller memory | verified | Native C11/C++17 installed consumers, arena lifecycle tests, the public ABI audit, and [`ADR-0001`](../adr/ADR-0001-c11-portable-core.md) / [`ADR-0002`](../adr/ADR-0002-caller-provided-memory.md) pass. |
| 3 | Same core sources build for Native, Wasm, and ESP-IDF | verified | The exact candidate passes Windows/Linux Native, all 108 Emscripten build actions, and all four ESP-IDF images; see [`QUALITY_GATES.md`](QUALITY_GATES.md) and [`M9_ESP32_EVIDENCE.md`](../hardware/M9_ESP32_EVIDENCE.md). |
| 4 | Complete 30-key music semantics | verified | Native/Wasm event-trace conformance, randomized properties, exact two-hour transport, and music/gesture/arpeggiator tests are recorded in [`QUALITY_GATES.md`](QUALITY_GATES.md). |
| 5 | Eighteen playable built-in instruments | verified | All 18 compiled data-driven procedural patches pass differentiated, calibrated audio analysis in [`M3_AUDIO_EVIDENCE.md`](../audio/M3_AUDIO_EVIDENCE.md). |
| 6 | Realtime-safe Chorus, Delay, Reverb, and Limiter | verified | DSP/effect unit, property, golden-audio, sanitizer, and Tiny/Standard/Full gates pass; the callback constraints are documented in [`REALTIME_SAFETY.md`](../audio/REALTIME_SAFETY.md). |
| 7 | Recording, playback, Mol Sequence, MIDI, and WAV tools | verified | Deterministic round trips, corruption rejection, three WAV formats, fuzzing, and the shared fixture pass in [`M4_SEQUENCE_EVIDENCE.md`](../sequence/M4_SEQUENCE_EVIDENCE.md). |
| 8 | Windows, Linux, and macOS headless service and CLI | implementation verified; external acceptance open | Windows and Linux independent daemon/CLI lifecycles, Startup/systemd integration, and packages pass. The macOS implementation and launchd simulation pass, but an Apple SDK/macOS execution is still required; see [`M5_DESKTOP_EVIDENCE.md`](../service/M5_DESKTOP_EVIDENCE.md). |
| 9 | Offline AudioWorklet + Wasm Web/PWA | verified | Production bundle, offline install/reload, Wasm DSP, AudioWorklet, MessagePort/SAB, keyboard/touch, lifecycle, and supported-browser automation pass in [`M6_WEB_EVIDENCE.md`](../web/M6_WEB_EVIDENCE.md). Safari qualification remains part of item 21. |
| 10 | Android Oboe and legal background audio | implementation verified; external acceptance open | Dual-ABI builds and the Android 15 headless-emulator AAudio, 30-key, focus, notification, background, screen-off, and cleanup gate pass. Physical routes and device lifecycle remain open in [`M7_ANDROID_EVIDENCE.md`](../mobile/M7_ANDROID_EVIDENCE.md). |
| 11 | iOS AudioUnit/AVAudioSession and legal background audio | implementation verified; external acceptance open | The complete application, portable production lifecycle/key state machines, sanitizers, and fail-closed Simulator job exist. Apple SDK, Simulator execution, and physical lifecycle/routes remain open in [`M7_IOS_EVIDENCE.md`](../mobile/M7_IOS_EVIDENCE.md). |
| 12 | HarmonyOS OHAudio and official continuous task | implementation verified; external acceptance open | Production ArkTS policy, Node-API/OHAudio host simulation, AArch64 execution, and public OpenHarmony API 12 compatibility HAPs pass. Formal authenticated DevEco/HarmonyOS construction and device runtime remain open in [`M8_HARMONY_EVIDENCE.md`](../mobile/M8_HARMONY_EVIDENCE.md). |
| 13 | ESP32/ESP32-S3 I2S and supported-chip A2DP Source | implementation verified; external acceptance open | Four exact ESP-IDF images build and both real firmware images boot in Espressif QEMU. Physical I2S, original-ESP32 A2DP, USB/HID/GPIO, recovery, and 30-minute boards remain open in [`M9_ESP32_EVIDENCE.md`](../hardware/M9_ESP32_EVIDENCE.md). |
| 14 | Unified keyboard, touch, MIDI, GPIO, and program input model | implementation verified; external acceptance open | Shared command/gesture tests plus Web, Android-emulator, desktop Raw Input/MIDI, and ESP32 production-queue execution pass. Physical mobile, MIDI, and GPIO endpoints remain part of the platform acceptance rows in [`PLATFORM_MATRIX.md`](PLATFORM_MATRIX.md). |
| 15 | Truthful Bluetooth speaker/keyboard capability behavior | implementation verified; external acceptance open | Runtime capability splits and unsupported states are tested; ESP32-S3 excludes Classic A2DP. Physical OS Bluetooth routes, HID peripherals, and original-ESP32 A2DP interoperability remain open in [`KNOWN_LIMITATIONS.md`](KNOWN_LIMITATIONS.md). |
| 16 | Allocation-free, nonblocking audio callbacks | verified | Source audits, callback instrumentation, properties, sanitizers, ThreadSanitizer, and [`REALTIME_SAFETY.md`](../audio/REALTIME_SAFETY.md) cover the complete render/callback boundary. |
| 17 | Parser boundary tests and fuzzing | verified | ASan/UBSan passes all 12 required parser/capture fuzzers and negative corpora; commands and scope are in [`TEST_PLAN.md`](../testing/TEST_PLAN.md). |
| 18 | Coverage, sanitizers, static analysis, and long tests | implementation verified; external acceptance open | Core coverage is 94.10% with every critical module above 95%; 44 production translation units, 57 sanitizer tests, 40 ThreadSanitizer tests, and optimized 30-minute/recovery tests pass. Physical mobile and ESP32 endurance is still required by platform acceptance; see [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md). |
| 19 | Locked dependencies, licenses, and complete SBOM | verified | The current dependency/license audit validates 10 tool/build components, 6 runtime dependencies, all snapshots/hashes, and the 17-package SPDX 2.3 SBOM. See [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md) and [`ASSET_LICENSES.md`](../legal/ASSET_LICENSES.md). |
| 20 | Reproducible documented clean-checkout builds | implementation verified; external acceptance open | Exact clean Windows, Linux, Wasm, Web, Android, and ESP-IDF builds pass with recorded package hashes. Native Apple and formal HarmonyOS clean builds require their unavailable official environments; see [`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md). |
| 21 | Every support claim backed by real evidence | open | Windows/Linux x64, supported Web browsers, Android emulator, AArch64 QEMU, OpenHarmony compatibility, and Espressif QEMU are qualified at their actual levels. Native macOS/Safari/ARM64, official iOS/HarmonyOS, physical mobile/ESP32/peripherals, and required route latency evidence are absent; see [`PLATFORM_MATRIX.md`](PLATFORM_MATRIX.md). |
| 22 | No old-project code or unknown assets | verified | The clean-room declaration, first-party Apache-2.0 headers, asset inventory/hashes, dependency lock, and notices contain no old-project or unknown-source asset claim. |
| 23 | Complete bilingual story and positioning | verified | [`README.md`](../../README.md) contains the required “张多少的键盘 / More or Less Zhang's Keyboard” story, product forms, quick starts, platform matrix, mappings, builds, privacy, and limitations. |
| 24 | Publish truthful `v1.0.0` with matching notes | open | [`v1.0.0.md`](../releases/v1.0.0.md) remains an explicit draft and the repository has no `v1.0.0` tag. Publishing is forbidden until items 8 and 10–21 close. |

## Release decision

The locally executable implementation, build, test, packaging, and simulation
work is green, but the complete Definition of Done is **not achieved**. The
remaining evidence requires an Apple host, an authenticated current HarmonyOS
toolchain/session, physical Android/iOS/Harmony/ESP32 targets and peripherals,
and synchronized physical latency capture. Emulation and controlled platform
models remain labeled as such and cannot close those rows.
