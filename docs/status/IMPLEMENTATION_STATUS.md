# Implementation Status

## Current milestone

The desktop application and headless-service matrix remains the first priority,
followed by mobile and ESP32. The production Web UI controls real Windows and
Linux daemon processes, and both platforms pass their complete native suites.
The exact macOS IOHID and CoreAudio-selected production sources also pass
controlled lifecycle simulations under two non-Apple compilers; this does not
replace an Apple SDK or macOS runtime result. With those locally reachable
desktop/service gates exhausted, both real ESP-IDF firmware images now also
execute through their production storage, input/control, shared-core, and
FreeRTOS audio paths in Espressif QEMU. Other locally actionable M10 release
gates are complete. Native and Wasm
regression, Release+LTO Tiny/Standard/Full profiles, static/shared ABI
verification, coverage, static analysis, ASan/UBSan with all eleven fuzzers,
Linux ThreadSanitizer, optimized endurance, release-size budgets, dependency/license
and SBOM audits, Windows/Linux package audits, Android packaging, and clean
checkout reproduction pass. Complete Windows ARM64 and Linux AArch64 desktop
products now cross-build through checked-in presets, closing their local build
gap. Linux AArch64 also passes an end-to-end QEMU product gate and 59/59 target
tests, while execution on native ARM64 hosts remains unclaimed. Android
emulator coverage now also stops and reopens AAudio across injected transient
focus loss/gain. M0 through M9 are implementation-complete, but the Definition
of Done is not complete: native
ARM64 runtime, Apple and Harmony toolchains, current Safari, physical mobile
devices, ESP32 hardware, physical audio routes, and instrumented end-to-end
latency remain external acceptance gates. No `v1.0.0` tag exists.

## Last verified commit

`dbc4374` (`test(esp32): execute firmware in Espressif QEMU`) is the latest
locally validated code candidate. Its ESP32 and ESP32-S3 images boot under
Espressif QEMU, mount/format transactional storage, pass the shared sequence
and C4 checks, drain 12 production input commands, and render more than 100,000
frames with non-silent finite output and zero project failure counters. All
four physical-board configurations still compile with the emulator option
disabled. Windows MSVC Release and Linux x86_64 GCC pass 79/79 tests; system
Chrome on Windows and bundled Chromium on Linux each pass the five applicable
desktop application cases, including a real platform daemon process and
authenticated service controller. The preceding `61b3342` candidate adds the
exact-production-source CoreAudio lifecycle simulation, and `18e6e7d` adds the
corresponding IOHID lifecycle simulation. The earlier `240b207` candidate's
dual-ABI Debug and instrumentation APKs, unsigned Release/R8/lintVital package,
and full lint gate
all pass. Android 15/API 35 emulator instrumentation also passes with injected focus loss/gain
and resumed AAudio callbacks. The prior `b3b7e14` candidate's
Linux AArch64 Release products pass the QEMU lifecycle/render gate, its AArch64
Debug suite passes 59/59, affected MSVC tests pass 5/5, and ESP32 HIL
parser/model self-tests pass 5/5. The prior
`4f77f56` candidate retains the complete MSVC Debug/LTO Release, Linux Clang,
ABI, sanitizer, analysis, endurance, size, package, and clean-checkout evidence
documented below. MSVC Debug/LTO Release and Linux Clang previously passed
78/78 tests; Emscripten MinSizeRel passes 31/31. Windows and Linux shared-core
builds pass 74/74 public-boundary tests, expose exactly the 47 version 1.0 API
symbols, and Linux ABI Compliance Checker reports 100% binary and source
compatibility with zero problems. GNU 15 Release+LTO Tiny, Standard, and Full
profiles pass 75/75, 76/76, and 75/75. ASan/UBSan passes 45/45 including all
eleven fuzzers. LLVM-MinGW 20260826/Clang 23.1.0 and GNU 15.2.0 produced the
complete Windows ARM64 and Linux AArch64 products. QEMU evidence is not inferred
to be native ARM64 or physical-device evidence. Validation ran on 2026-09-03.

## Completed requirements

- The repository baseline, clean-room statement, Apache-2.0 licensing, privacy
  policy, bilingual introduction, CMake profiles, warnings-as-errors, dependency
  audit, locked miniaudio/Oboe sources, notices, and SPDX SBOM are present.
- `mol_core` is ISO C11 with a stable public C ABI and caller-owned aligned
  storage. Its render path has no allocation, blocking, logging, or language
  runtime callback.
- Static and shared builds install the same `mol::core` package. Shared builds
  hide every internal symbol and export exactly 47 `MOL_API` functions. The
  checked-in symbol and ABI Dumper baselines are enforced by a three-OS shared
  CI matrix and Linux ABI Compliance Checker.
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
- `mol-latency-probe` analyzes bounded multichannel PCM16 physical captures,
  requires route/device/buffer/commit metadata, and emits the capture SHA-256,
  sorted individual observations, P50/P95/maximum, unmatched counts, and an
  optional fail-closed P95 limit. Its deterministic 20-event fixture produces
  19.5/28.05/29 ms, while corrupt input and a 20 ms P95 limit are rejected.
- Patch, Mol Sequence, service configuration, JSON-RPC, MolWireEventV1, MIDI,
  latency/audio captures, ESP32 settings/Web forms, and HID-report Clang
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
- Checked-in cross toolchains build that complete desktop product, including
  `mol-keyboardd`, `molctl`, `mol-play`, `mol-render`, `mol-seq`,
  `mol-patchc`, `mol-audio-analyze`, and `mol_core`, as Windows ARM64 COFF and
  Linux AArch64 ELF. CI also includes native Windows and Ubuntu ARM64 runners.
- QEMU 10.2.1 executes the Linux AArch64 Release daemon, CLI, and renderer as a
  fail-closed product gate. A separate Debug target build passes 59/59 tests,
  including the 18-preset audio golden, local IPC, nested daemon/renderer
  processes, null playback, latency analyzer, and C/C++ consumers.
- The production desktop Web UI passed its applicable system-Chrome run on
  Windows and bundled-Chromium run on Linux. In both cases its service controller
  authenticated to the real platform daemon, delivered keyboard events,
  recorded a sequence, rejected a wrong token, and shut the daemon down cleanly.
- The exact macOS IOHID and CoreAudio-selected production sources compile and
  execute against controlled API models under MSVC and Linux Clang. Input
  enumeration/gesture cleanup and audio callbacks/device recovery pass, without
  claiming Apple SDK or native macOS evidence.
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
  matrix passed 15 applicable cases with 27 explicit capability skips across
  42 project/test combinations. Chrome, Edge, and Chromium mobile exercised
  realtime AudioWorklet output; Firefox exercised the same worklet/Wasm DSP in
  an `OfflineAudioContext` because its headless Windows process exposes no
  realtime audio output device. Detailed evidence and the unclaimed Safari
  boundary are in
  `docs/web/M6_WEB_EVIDENCE.md`.
- The Android application packages that same complete local UI without network
  permission, uses a bounded allow-listed JS bridge, and runs the shared core
  through JNI and Oboe in a legal `mediaPlayback` foreground service. Debug,
  unsigned Release, instrumentation, lint, both required ABIs, configurable
  application ID, privacy/notices packaging, route/focus handlers, foreground
  hardware keys, and private recording persistence are implemented.
- The Android 15 x86_64 emulator exercised the real packaged UI-to-AAudio path
  at 48 kHz with zero render/non-finite failures. Callback count advanced from
  98 in background to 203 with the screen off, then the idle background stream
  and foreground state stopped. Injected transient focus loss stopped AAudio;
  focus gain reopened it and resumed finite callbacks. Detailed evidence and
  physical-device boundaries are in `docs/mobile/M7_ANDROID_EVIDENCE.md`.
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
  atomic recording persistence, and route/interruption restoration. The official
  OpenHarmony 5.0.0.71/API 12 public SDK now builds and audits Debug and Release
  compatibility HAPs with ArkTS bytecode plus AArch64 and x86-64 native audio
  libraries. This is build evidence for the compatibility product; no formal
  DevEco/HarmonyOS build or runtime result is claimed here.
- The M9 firmware now provides a configurable 5x6 GPIO matrix; shared BLE HID
  on both targets; Classic HID and A2DP Source/AVRCP on ESP32; USB boot HID on
  ESP32-S3; NVS settings and transactional FAT sequences; persisted HID/A2DP
  peers; physical configuration, clear-pairing, and factory-reset gestures; an
  isolated bounded control task; and an optional physically authorized WPA2
  SoftAP Web configuration service with strict Origin/token/form validation.
- ESP-IDF 6.1 built the default ESP32/ESP32-S3 images at 1,018,256 and 796,832
  bytes and the 4 MiB Web variants at 1,551,168 and 1,302,192 bytes. The core
  archive remains below 28 KiB and the queried eight-voice engine uses 37,664
  bytes of its 37,888-byte arena. A host-tested HIL verifier now fails on reset,
  underrun, watchdog, queue, persistence, capability, input, or real I2S-capture
  violations. Its virtual-clock mode passes 180 healthy snapshots for each
  chip family and rejects reset, deadline, stalled-audio, and firmware-error
  injection without claiming board execution.
- Isolated QEMU configurations for both chips execute the real ESP-IDF 6.1
  bootloader/application, NVS/FAT startup, the shared sequence and C4 checks,
  the production input/control path, and the statically allocated FreeRTOS
  audio task through a paced virtual PCM sink. The runner rejects missing
  phases, project errors, non-finite or silent audio, physical-peripheral
  startup, and failure counters, then writes `emulated-firmware` evidence with
  explicit physical and real-time exclusions.

## In-progress work

- The desktop-first local audit is complete: Windows/Linux application and
  service process paths pass, while macOS platform-specific source paths have
  executable simulations. Native macOS application/service acceptance is the
  highest-priority external gate. The strongest reachable device-free ESP32
  firmware execution gate also passes; mobile and ESP32 external acceptance
  follows the desktop gate. Documentation remains a draft and `v1.0.0` remains
  forbidden until all results pass.

## Blocked platform checks

- Apple SDKs and DevEco/HarmonyOS SDKs are not available on this Windows host.
  The public OpenHarmony SDK compatibility build and macOS simulations do not
  change those formal platform constraints or runtime status.
- Playwright's Windows WebKit port does not expose AudioWorklet and is not actual
  Safari. Current-stable Safari remains unverified until run on an Apple host.
- No Bluetooth output was exposed for the Windows run. WSL exposes neither a
  physical evdev keyboard nor native Linux audio hardware. Those M5 acceptance
  paths and macOS compilation/runtime remain unverified.
- Windows ARM64 and Linux AArch64 binaries are build-verified, but no native
  ARM64 host was available locally. Linux AArch64 QEMU execution covers target
  instructions, service IPC, rendering, and tests, but cannot verify native
  scheduling, input, audio, latency, or hardware lifecycle. Native ARM64 CI jobs
  are configured but an unpushed local commit is not reported as a CI result.
- Physical Android/Apple/Harmony/ESP32 devices, I2S capture equipment, signing
  credentials, and long-run device time are not available. The Android emulator
  result, ESP-IDF builds, and Espressif QEMU firmware runs are not promoted to
  device verification.

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

MSVC 19.51.36248 passed 78/78 tests in Debug and LTO Release. These runs include
the strict Web form protocol and HIL evidence-parser tests in addition to the
independent daemon process, realtime runtime, local IPC, all service methods,
CLI validation, configuration restart, recording/playback, the macOS
IOHID/CoreAudio lifecycle simulations, and prior core/tool coverage. Dedicated
GNU 15 Release+LTO presets previously passed 75/75 for Tiny, 76/76 for
Standard, and 75/75 for Full. The Full run exercises 64 voices, 4,096 sequence
events, the complete desktop daemon, and the expanded fixed host arenas.

Under WSL, Linux x86_64 Clang 21.1.8 built the desktop service and passed 78/78
tests. The daemon process used its null sink and a private Unix socket; the
production Web UI additionally controlled it under bundled Chromium. Physical
Linux devices remain unclaimed.

Static/shared API parity is checked separately:

```sh
cmake --preset ci-shared
cmake --build --preset ci-shared
ctest --preset ci-shared --output-on-failure
nm --dynamic --defined-only --format=posix \
  build/ci-shared/libmol_core.so.0.1.0 \
  | cut -d ' ' -f 1 | LC_ALL=C sort > build/mol_core-current.symbols
diff -u abi/mol_core-1.0.symbols build/mol_core-current.symbols
abi-dumper build/ci-shared/libmol_core.so.0.1.0 \
  -lver current -o build/mol_core-current.dump
abi-compliance-checker -l mol_core -old abi/mol_core-1.0.dump \
  -new build/mol_core-current.dump \
  -report-path build/mol_core-abi-report.html
```

Windows and Linux shared builds passed 74/74 tests. The current dynamic library
matches all 47 expected symbols exactly; ABI Compliance Checker reports 100%
binary and source compatibility, zero problems, and zero warnings. Independent
installed C11 and C++17 consumers also compile and execute against the Windows
shared package.

With the pinned Emscripten environment active:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug --output-on-failure
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release --output-on-failure
```

Emscripten 6.0.5 and Node.js 22.16.0 passed 31/31 tests in Debug and LTO
MinSizeRel. Both configurations match the Native event, sequence-fixture, and
18-preset audio-metric goldens.

The production Web bundle passed 12/12 Node tests. Playwright 1.62.1 ran 42
browser project/test combinations: 15 applicable cases passed and 27 were
explicitly skipped by capability. System Chrome 151.0.7922.175, system Edge
152.0.4191.53, Firefox 153.0, Chromium 151.0.7922.34 mobile emulation, and
WebKit 26.5 desktop/mobile rendering were covered. Chrome and Edge exercised
the realtime AudioWorklet; Firefox executed the real worklet and Wasm DSP in
an offline audio graph because the headless runner exposes no realtime output
device. Chrome also reloaded offline, started audio, played a note, and observed
the core event. Actual Safari is not claimed.

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

The reproducible Debug pipeline passed 31/31 Wasm tests, 12/12 Web tests,
TypeScript strict checking, the production UI build, and the dual-ABI Android
build. Debug, unsigned Release, device-test APKs, R8/lintVital, and full Debug
lint all passed. Archive inspection confirmed the paired
`mol_audio_worklet_core.js` and `.wasm` assets in both final APK variants. The
official Android 15/API 35 x86_64 emulator returned AAudio
API 2, 48 kHz, 32,640 rendered frames at the foreground checkpoint, a stopped
runtime after injected transient focus loss, 2 callbacks after focus-gain
reopen, 98 background callbacks, 203 screen-off callbacks, no
render/non-finite failure, successful idle shutdown, and instrumentation code
`-1`.

For sanitizer and parser fuzz validation, activate the Visual Studio environment
and place Clang 22 on `PATH`:

```powershell
cmake --preset fuzz-clang
cmake --build --preset fuzz-clang
ctest --preset fuzz-clang --output-on-failure
```

The ASan/UBSan configuration passed 45/45 tests. Patch, Mol Sequence, service
configuration, JSON-RPC, MolWireEventV1, MIDI, latency/audio captures, ESP32
settings/Web forms, and HID-report libFuzzer smoke sessions each ran for 20
seconds and produced no finding. Accepted MIDI inputs are also reparsed and
compared through the canonical sequence JSON representation.

The latency analyzer's five native tests pass in static and shared suites. The
synthetic fixture checks its 20 raw observations and 19.5/28.05/29 ms
P50/P95/maximum, but is explicitly not physical product evidence. Real route
commands and fail-closed acquisition rules are in
`docs/testing/LATENCY_MEASUREMENT.md`; all hardware rows remain open.

With the pinned ESP-IDF environment active, from `platforms/esp32`:

```powershell
./build-target.ps1 -Target esp32
./build-target.ps1 -Target esp32s3
./build-target.ps1 -Target esp32 -WebUi
./build-target.ps1 -Target esp32s3 -WebUi
```

All four ESP-IDF 6.1/GNU 15.2.0 variants rebuilt successfully. Default/Web
image sizes are 1,018,256/1,551,168 bytes for ESP32 and
796,832/1,302,192 bytes for ESP32-S3. Default ESP32 reports 101,892 of 124,580
bytes DRAM; its Web variant reports 118,000 bytes. Default ESP32-S3 reports
148,923 of 341,760 bytes DIRAM; its Web variant reports 187,975 bytes. These
are build/map results, not physical playback results.

The optional Espressif QEMU 9.2.2 gate executed the clean `dbc4374` images:

```powershell
platforms/esp32/run-qemu.ps1 -Target esp32
platforms/esp32/run-qemu.ps1 -Target esp32s3
```

ESP32 passed after 105,728 frames, 12 drained commands, and 211,256 nonzero
samples; ESP32-S3 passed after 101,888 frames, 12 commands, and 203,852 nonzero
samples. Both had three snapshots, 262.5 Hz C4, zero non-finite samples, and
zero project failure counters. Each emulator run counted two deadline misses;
the 22,459/26,857 microsecond maximum render times are reported but excluded from
real-time acceptance. Full hashes and physical exclusions are in
`docs/hardware/M9_ESP32_EVIDENCE.md`.

Additional locally actionable M10 gates passed with these reproducible command
families:

```sh
cmake --preset coverage-clang
cmake --build --preset coverage-clang
python3 tools/coverage_gate.py --build-dir build/coverage-clang --source-dir .
cmake --preset static-analysis-clang
cmake --build --preset static-analysis-clang
python3 tools/static_analysis.py --build-dir build/static-analysis-clang
cmake --preset tsan-clang
cmake --build --preset tsan-clang
ctest --preset tsan-clang
cmake --preset endurance
cmake --build --preset endurance
ctest --test-dir build/endurance --output-on-failure -L endurance
```

The complete desktop products also pass the checked-in cross-build presets:

```sh
# Ubuntu/Debian system cross compiler, or an unpacked sysroot selected through
# MOL_LINUX_AARCH64_ROOT.
cmake --preset ci-linux-aarch64
cmake --build --preset ci-linux-aarch64
```

With QEMU user mode and the AArch64 runtime prefix available, the target test
binaries and final products execute under emulation:

```sh
cmake --preset ci-linux-aarch64-qemu
cmake --build --preset ci-linux-aarch64-qemu
ctest --preset ci-linux-aarch64-qemu
python3 tools/aarch64_emulation_gate.py \
  --qemu /usr/bin/qemu-aarch64 --sysroot /usr/aarch64-linux-gnu \
  --build-dir build/ci-linux-aarch64 \
  --artifact-commit "$(git rev-parse HEAD)" \
  --report build/aarch64-emulation-report.json
```

The local QEMU 10.2.1 run passed 59/59 AArch64 tests in 92.91 seconds. The
Release product report for `b3b7e14` passed daemon/CLI IPC, record/playback,
doctor, self-test, finite benchmark, clean shutdown, and deterministic WAV
validation. It is explicitly `simulated-runtime`, not native/device evidence.

```powershell
# Extract LLVM-MinGW 20260826 UCRT, then select it explicitly.
$env:MOL_LLVM_MINGW_ROOT = "C:\path\to\llvm-mingw-20260826-ucrt-x86_64"
cmake --preset ci-windows-arm64-cross
cmake --build --preset ci-windows-arm64-cross
```

The Linux result contains AArch64 ELF executables; the Windows result contains
COFF-ARM64 executables and archive members. Both were inspected with target
object tools after the build. The LLVM-MinGW archive used locally was
190,721,391 bytes with SHA-256
`ae601f4e0f72bbdf441ad2df8bb16f037e2e9251559ea6b37b4057aef39c06c3`.

Coverage passed at 94.10% overall, including 95.95% queue/memory, 97.78%
music-state, 97.49% Patch, and 95.57% Sequence coverage. Clang static analysis
passed all 38 first-party production translation units. Linux Clang
ThreadSanitizer passed 40/40 tests in 15.62 seconds. The refreshed GCC 15
optimized endurance suite passed 2/2 in 270.38 seconds: the engine simulated
1,800 seconds in 268.567 seconds (6.70x realtime, approximately 14.92% of one
core), emitted 230,136 events, and produced no non-finite samples. Runtime
recovery completed 30 rebuild cycles in 1.42 seconds.

The refreshed release size gate passed at 505,468 bytes for the stripped core,
943,392 bytes for daemon plus CLI, 22,978 bytes for gzip-compressed Wasm, and
157,413 bytes for deployable Web resources. Dependency locks, notices,
licenses, npm audit, and SPDX SBOM validation passed. CPack package audits each found 146
required files, including `mol-latency-probe`, and passed installed daemon/CLI
smoke tests: the Windows AMD64 ZIP is 1,291,580 bytes with SHA-256
`0d419cce06880e24ca871548bbbbe8f1a4ef0f59c38f56bd75baf2157907b7ab`;
the Linux x86_64 TGZ is 1,676,870 bytes with SHA-256
`255bc2069c9d33b8506f2d03d9d8732a26295308ebdb6dd7862202e95e6b5492`.
They are unsigned 0.1.0 candidate archives built from `4f77f56`, not releases.

```sh
python3 tools/release_size_gate.py \
  --native-core build/package-release/libmol_core.a \
  --daemon build/package-release/apps/mol-keyboardd/mol-keyboardd \
  --cli build/package-release/apps/molctl/molctl \
  --wasm build/wasm-release/platforms/web/emscripten/mol_core.wasm \
  --web-dir apps/web/dist --report-dir build/size-report
cpack --config build/package-release/CPackConfig.cmake -B build/packages
python3 tools/package_audit.py --archive <archive> \
  --report-dir <report-directory> --expected-version 0.1.0
```

A clean local clone of `d0fa3d6f96fe0df9e5dd61ae944bc5a9e1e6e030` with no
copied repository build output or managed-dependency directory was built in a
new directory. MSVC LTO Release passed 79/79 tests; Emscripten MinSizeRel passed
32/32; a clean `npm ci` reported zero vulnerabilities, the Web unit suite
passed 12/12, TypeScript strict checking passed, and the production bundle
built. Fresh ESP32 and ESP32-S3 QEMU builds used the external official ESP-IDF
6.1 installation, booted both firmware images, and passed their three-snapshot
runtime gates with 107,904 and 104,960 rendered frames respectively, 12 input
commands each, finite non-silent output, and zero render or write failures. The
Emscripten configure used the Ninja executable shipped with Visual Studio
because Ninja is not on this host's default `PATH`.

## Known failures

No locally reproducible implementation, build, test, sanitizer, analysis,
package, or documentation failure is currently known. The unavailable external
acceptance runs listed above remain open gates rather than skipped or passing
tests.

## Known environment constraints

- `cl`, Emscripten, ESP-IDF, and the Clang ASan runtime are activated through
  their toolchain environments and are not all on the default `PATH`.
- Cross-platform source checks are not promoted to device verification.

The HarmonyOS application descriptors, project audit, and native source-check
boundary pass locally. Windows MSVC Release and Linux GCC pass 79/79 tests. The
official OpenHarmony 5.0.0.71/API 12 public SDK and Hvigor 5.8.9 build and audit
both Debug and Release compatibility HAPs; the Release artifact is an unsigned
2,951,842-byte package containing ArkTS bytecode and both required native ABIs.
`platforms/harmony/build-app.sh release` remains the fail-closed formal DevEco
lane. Detailed evidence and pending formal/device acceptance are in
`docs/mobile/M8_HARMONY_EVIDENCE.md`.

## Next highest-priority task

Run `cmake --preset ci-macos`, its build, and all CTest cases on a real macOS
host first; then exercise the production Web UI against that daemon and current
Safari, including CoreAudio, IOHID permission, launchd, route loss, and clean
shutdown. After the desktop gate, run native Windows/Linux ARM64 execution,
official iOS and Harmony builds, physical Android/iOS/Harmony lifecycle and
route checks, ESP32/ESP32-S3 30-minute HIL with I2S/A2DP/USB/GPIO evidence, and
instrumented P50/P95/maximum latency on every required route. Keep `v1.0.0`
blocked until all results pass and the exact final candidate is reviewed.
