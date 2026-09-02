# Implementation Status

## Current milestone

M9 ESP32 product work is active. The Android, iOS, and HarmonyOS application
implementations are complete; Android is dual-ABI build-verified and
runtime-verified on an Android 15 x86_64 emulator, while iOS and HarmonyOS
remain source-reviewed because their platform SDKs are unavailable on this
host. Physical Bluetooth/evdev, macOS/Safari, and physical mobile acceptance
remain environment-blocked. M0 through M4, the M5 desktop implementation, and
the M6 Web/PWA implementation are complete.

## Last verified commit

`ae80d9c` (`feat(harmony): add native ArkUI application`) is the last locally
validated product-code commit. The HarmonyOS native source boundary, project
audit, and unaffected portable targets passed locally; the HarmonyOS target
itself is not called build-verified. The validation below was run on
2026-09-03. Documentation changes do not alter binaries.

## Completed requirements

- The repository baseline, clean-room statement, Apache-2.0 licensing, privacy
  policy, bilingual introduction, CMake profiles, warnings-as-errors, dependency
  audit, locked miniaudio/Oboe sources, notices, and SPDX SBOM are present.
- `mol_core` is ISO C11 with a stable public C ABI and caller-owned aligned
  storage. Its render path has no allocation, blocking, logging, or language
  runtime callback.
- The same core sources build and run under MSVC, Emscripten, and ESP-IDF. C and
  C++17 consumers link against the public API.
- The M1 path renders measured C4 through Native, WebAssembly/AudioWorklet,
  desktop miniaudio, and the ESP32 core/I2S host. Android Oboe, Apple AudioUnit,
  and Harmony OHAudio native entries use the shared fixed-memory callback
  runtime; Apple and Harmony SDK/device validation remains explicitly unclaimed.
- M2 implements the 30-key map, octave and transpose, eight scales and twelve
  tonics, ten chord modes, sustain, six deterministic arpeggiator modes and six
  rates, monophonic portamento, transport, time signatures, and metronome. The
  Native/Wasm event golden is 35 events with digest `9e6cebee9d02f409` and
  transport frame 50,000. Two-hour rational transport reaches frame 345,600,000
  with zero cumulative drift.
- M3 provides the complete portable DSP primitives, six real synthesis model
  families, 18 strict bilingual JSON Patches, a deterministic fixed-layout
  Patch compiler/decoder, embedded compiled Patches, graceful and hard preset
  switching, deterministic voice stealing, Chorus, Delay, Reverb, mixer, DC
  blocking, limiting, parameter smoothing, and transition ramps.
- Every preset renders finite non-silent distinguishable output from C3 through
  C7. Rapid 64-frame hard switches through all 18 presets remain below the 0.25
  sample-step threshold. The Tiny profile exposes every preset ID.
- The fixed Standard golden sequence calibrates all 18 integrated RMS values to
  0.011469–0.012139. Native and Wasm compare peak, RMS, DC, stereo difference,
  step, attack, active end, spectral centroid, and three spectral bands using
  documented tolerances; tests never overwrite the golden.
- Mol Sequence v1 provides a 112-byte versioned header, fixed initial state,
  ULEB128 frame deltas, bounded metadata and records, unknown optional-record
  skipping, CRC32 completion record, forward-only callbacks, deterministic
  writing, and explicit incomplete/corrupt-file failure. The example binary is
  225 bytes with SHA-256
  `415009bcd666fe8e277a15cb871ee2c3dba2bf73c3b168fcb8ed8b69ddcbb39a`.
- Recording captures the post-mapping voice events and initial effect state in
  caller-owned bounded storage. Playback rescales the sequence time base and is
  bit-identical across repeated fresh and reused engine runs.
- `mol-seq` validates and converts binary/strict JSON, imports and exports SMF
  type 0/1 MIDI with tempo maps, and performs non-destructive trim, merge, and
  quantize operations. Binary/JSON and MIDI round trips are deterministic;
  malformed and truncated inputs fail safely.
- `mol-render` renders manual notes or `.molseq` input without an audio device
  to PCM16, PCM24, or float32 WAV; supports mono/stereo, 8-192 kHz custom rates,
  and normal or 2x high-quality rendering; and emits duration, peak, RMS,
  clipped, NaN/Inf, underrun, SHA-256, and JSON-report evidence. CTest checks
  WAV headers and independently recomputes each report hash.
- Patch and Mol Sequence parser/writer, MolWireEventV1, and JSON-RPC Clang
  libFuzzer entries cover arbitrary bounded input and successful-parse round
  trips where applicable. ASan/UBSan builds all portable and control-plane
  tests; the Windows ASan runtime is deployed for all test binaries.
- Native and Wasm parse the exact same generated 12-event sequence fixture and
  match one golden summary. The ESP32 and ESP32-S3 startup paths parse those
  same checked-in bytes before I2S starts; both firmware targets compile. The
  current app binaries are 153,440 and 179,328 bytes respectively. Detailed M4
  evidence is in `docs/sequence/M4_SEQUENCE_EVIDENCE.md`.
- The desktop product now includes `mol-keyboardd`, `molctl`, miniaudio output
  and hotplug recovery, Windows Raw Input/Linux evdev/macOS IOHIDManager input
  adapters, bounded local IPC, all 41 specified JSON-RPC methods, strict atomic
  configuration, recording/playback, actionable doctor/self-test/benchmark,
  and user startup assets for Windows, systemd, and launchd.
- An independent Windows Release process used the active 48 kHz stereo WASAPI
  device, exposed the physical Raw Input adapter, passed doctor and a 96,000
  frame benchmark at 80.68 times realtime with no non-finite samples, and shut
  down with exit code 0. Linux/Clang ran the service lifecycle over a private
  Unix socket under WSL. Detailed evidence and unclaimed hardware boundaries
  are in `docs/service/M5_DESKTOP_EVIDENCE.md`.
- The complete bilingual Web/PWA product provides Explore and Studio controls,
  real AudioWorklet/Wasm synthesis, MessagePort and SharedArrayBuffer command
  paths, keyboard/multitouch gestures, IndexedDB recordings, offline service
  worker, and an authenticated loopback desktop-service controller. The browser
  matrix passed 15 applicable cases with 21 explicit capability skips. Detailed
  evidence and the unclaimed Safari boundary are in
  `docs/web/M6_WEB_EVIDENCE.md`.
- The Android application packages that same complete local UI without network
  permission, uses a bounded allow-listed JS bridge, and runs the shared core
  through JNI and Oboe in a legal `mediaPlayback` foreground service. Debug,
  unsigned Release, instrumentation, lint, both required ABIs, configurable
  application ID, privacy/notices packaging, route/focus handlers, foreground
  hardware keys, and private recording persistence are implemented.
- The Android 15 x86_64 emulator exercised the real packaged UI-to-AAudio path
  at 48 kHz with zero render/non-finite failures. Callback count advanced from
  162 in background to 263 with the screen off, then the idle background stream
  and foreground state stopped. Detailed evidence and physical-device
  boundaries are in `docs/mobile/M7_ANDROID_EVIDENCE.md`.
- The iOS source now packages the same production UI with an offline-only
  WKURLSchemeHandler, Promise reply bridge, exact request schema, allow-listed
  commands, bounded event/recording transfer, foreground UIKit HID mapping,
  private atomic `.molseq` persistence, privacy manifest, localized metadata,
  and app icon. Its Objective-C++ controller restores persistent engine state
  after route/interruption/media-service rebuilds and keeps background audio
  only for playback or a running metronome transport. Xcode simulator/device
  pipelines are checked in, but no Apple build is claimed on this host.
- The HarmonyOS Stage application provides the complete ArkUI surface and exact
  30-key foreground mapping, strict Node-API controls, OHAudio fast request with
  normal fallback and effective latency reporting, AudioSession focus,
  AVSession media commands, official audio-playback continuous tasks, private
  atomic recording persistence, and route/interruption restoration. Its HAP
  pipeline audits real package contents, but no DevEco build is claimed here.

## In-progress work

- M9: extend the existing build-verified ESP32/ESP32-S3 I2S baseline into the
  complete device product, starting with the remaining portable GPIO/input,
  persistence, configuration, capability, and hardware-test infrastructure.

## Blocked platform checks

- Apple SDKs and DevEco/HarmonyOS SDKs are not available on this Windows host.
- Playwright's Windows WebKit port does not expose AudioWorklet and is not actual
  Safari. Current-stable Safari remains unverified until run on an Apple host.
- No Bluetooth output was exposed for the Windows run. WSL exposes neither a
  physical evdev keyboard nor native Linux audio hardware. Those M5 acceptance
  paths and macOS compilation/runtime remain unverified.
- Physical Android/Apple/Harmony/ESP32 devices, signing credentials, and
  long-run device time are not available. The Android emulator result is not
  promoted to device verification.

## Exact validation commands and results

From a Visual Studio 2026 x64 developer shell with Ninja on `PATH`:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug --output-on-failure
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release --output-on-failure
```

MSVC 19.51.36248 passed 63/63 tests in Debug and LTO Release. This includes an
independent daemon process, realtime runtime, local IPC, all service methods,
CLI validation, configuration restart, recording/playback, and prior core/tool
coverage. A separate Tiny profile passed 21/21; a Standard build with Chorus,
Delay, and Reverb all disabled passed 21/21.

Under WSL, Linux x86_64 Clang 21.1.8 built the desktop service and passed 63/63
tests. The daemon process used its null sink and a private Unix socket; physical
Linux devices remain unclaimed.

With the pinned Emscripten environment active:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug --output-on-failure
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release --output-on-failure
```

Emscripten 6.0.5 and Node.js 22.16.0 passed 24/24 tests in Debug and LTO
MinSizeRel. Both configurations match the Native event, sequence-fixture, and
18-preset audio-metric goldens.

The production Web bundle passed 12/12 Node tests. Playwright 1.62.1 ran 36
browser project/test combinations: 15 applicable cases passed and 21 were
explicitly skipped by capability. System Chrome 151.0.7922.175, system Edge
152.0.4191.53, Firefox 153.0, Chromium 151.0.7922.34 mobile emulation, and
WebKit 26.5 desktop/mobile rendering were covered. Actual Safari is not claimed.

With JDK 21, Android API 36, Build Tools 36.0.0, NDK 28.2.13676358, and CMake
3.31.6 installed:

```powershell
& "$env:EMSDK/emsdk_env.ps1"
platforms/android/build-app.ps1 Debug
$env:MOL_SKIP_WEB_BUILD = "1"
platforms/android/build-app.ps1 DebugAndroidTest
platforms/android/build-app.ps1 Release
Push-Location platforms/android
./gradlew.bat :app:lintDebug --no-daemon --no-configuration-cache
Pop-Location
```

The reproducible Debug pipeline passed 24/24 Wasm tests, 12/12 Web tests,
TypeScript strict checking, the production UI build, and the dual-ABI Android
build. Debug, unsigned Release, device-test APKs, R8/lintVital, and full Debug
lint all passed. The official Android 15/API 35 x86_64 emulator returned AAudio
API 2, 48 kHz, 36,480 rendered frames at the foreground checkpoint, 162
background callbacks, 263 screen-off callbacks, no render/non-finite failure,
successful idle shutdown, and instrumentation code `-1`.

For sanitizer and Patch fuzz validation, activate the Visual Studio environment
and place Clang 22 on `PATH`:

```powershell
cmake --preset fuzz-clang
cmake --build --preset fuzz-clang
ctest --preset fuzz-clang --output-on-failure
```

The ASan/UBSan configuration passed 30/30 tests. Patch, Mol Sequence,
MolWireEventV1, and JSON-RPC libFuzzer smoke sessions each ran for 20 seconds
and produced no finding.

With the pinned ESP-IDF environment active, from `platforms/esp32`:

```powershell
idf.py -B build-esp32 build
idf.py -B build-esp32 size-components
idf.py -B build-esp32s3 build
idf.py -B build-esp32s3 size-components
```

Both ESP-IDF 6.1/GNU 15.2.0 targets rebuilt successfully with the shared
sequence startup check. ESP32 uses 147,164 of 180,736 bytes reported internal
DRAM (33,572 free); ESP32-S3 uses 172,655 of 341,760 bytes reported internal
memory (169,105 free). These are build/map results, not physical playback
results.

## Known environment constraints

- `cl`, Emscripten, ESP-IDF, and the Clang ASan runtime are activated through
  their toolchain environments and are not all on the default `PATH`.
- Cross-platform source checks are not promoted to device verification.

The HarmonyOS application descriptors, project audit, and native source-check
boundary pass locally. MSVC Debug passed 65/65 tests after adding the complete
application and HAP audit. `platforms/harmony/build-app.sh release` fails closed
with an actionable DevEco requirement on this host, as intended. Detailed
evidence and pending physical acceptance are in
`docs/mobile/M8_HARMONY_EVIDENCE.md`.

## Next highest-priority task

Complete the locally actionable M9 ESP32 product requirements. Keep physical
ESP32/ESP32-S3, A2DP radio, I2S signal, and 30-minute hardware claims explicit
until matching boards and instruments are available.
