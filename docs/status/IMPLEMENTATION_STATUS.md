# Implementation Status

## Current milestone

M3 instruments, DSP, and effects. M0, M1, and M2 are complete.

## Last verified commit

`56b00f8` (`test(music): lock cross-target event semantics`) is the last
verified commit. Native Debug/Release and WebAssembly Debug/Release were rebuilt
and tested after that change on 2026-09-02.

## Completed requirements

- Project identity, clean-room statement, Apache-2.0 license, privacy policy,
  and bilingual introductory documentation are present.
- The required development branch is `codex/mol-keyboard-v1`.
- `mol_core` is ISO C11 and initializes entirely in aligned caller-owned memory.
- Debug and release builds pass warnings-as-errors with MSVC 19.51.36248.
- C and C++17 consumers link and run against the same public C ABI.
- Interleaved and planar rendering is allocation-free and advances frame time.
- The arena now contains fixed-capacity voices, scheduled commands, and events.
- A polyBLEP saw oscillator, ADSR envelope, deterministic voice allocation, and
  sample-accurate command heap produce real polyphonic audio.
- Automated analysis measured rendered MIDI C4 within 1 Hz of 261.6256 Hz,
  rejected NaN/Inf, verified release to silence, and exercised eight voices plus stealing.
- `mol-render` creates deterministic little-endian 16-bit PCM mono/stereo WAV
  files without opening an audio device and reports duration, peak, RMS,
  clipping, non-finite samples, and underruns.
- CTest validates the RIFF/WAVE header and verifies that rendered PCM is non-silent.
- Emscripten 6.0.5 compiles the same core and all portable C/C++ tests in Debug
  and LTO Release configurations.
- A fixed-memory ES module exposes the engine to workers and passed a Node.js
  Wasm C4 conformance test at 261.25 Hz with peak 0.24512254.
- The LTO Release Web artifacts are 3,897-byte Wasm and 9,862-byte JS loader files.
- A separate fixed-memory, single-file AudioWorklet artifact runs the same core
  without asynchronous module setup in the render scope. Node conformance and
  a real Chromium `OfflineAudioContext` both measured C4 at 261.25 Hz; the
  Release worklet artifact is 15,089 bytes.
- ESP-IDF 6.1 builds the exact core source as a Tiny-profile component for both
  ESP32 and ESP32-S3 with independently generated target configuration.
- Both firmware targets contain a bounded static-memory 32 kHz C4 conformance
  check, a configurable ESP-IDF standard I2S TX host, fixed DMA geometry,
  saturated PCM16 conversion, a statically allocated high-priority render task,
  watchdog integration, and periodic underrun/timing/memory diagnostics.
- ESP32 produced a 124,256-byte image with 2,660 bytes of mapped core flash
  code; ESP32-S3 produced a 149,520-byte image with 2,672 bytes of mapped core
  flash code. The firmware host reserves 22,168 bytes of static DRAM on each.
- An independent native 32 kHz PCM16 render measured the firmware's C4 sequence
  at 261.25 Hz with peak 0.19390869, zero non-finite samples, and zero underruns.
- miniaudio 0.11.25 is pinned by commit and archive hash with a preserved
  license, locked build options, third-party notice, SPDX 2.3 SBOM, dependency
  audit documentation, and an automated license-integrity test.
- `mol-play` enumerates desktop playback devices with reusable backend IDs,
  follows the system default, reports actual sample rate and native period
  geometry, and renders directly into an allocation-free device callback.
- Debug and LTO Release null-backend realtime tests measured C4 at 261.25 Hz
  with finite, non-silent output and zero render failures.
- A real Release WASAPI run opened the Windows default output at 48 kHz, handled
  109 callbacks/52,320 frames, measured C4 at 261.25 Hz, and reported zero
  render failures and non-finite samples. The reported three 480-frame native
  periods are a 30 ms buffer-duration estimate, not an end-to-end latency claim.
- A shared, fixed-memory platform audio runtime gives native hosts one C11
  command/render path and accepts variable callback sizes without allocation.
- Oboe 1.10.0 is pinned by exact commit, archive hash, license snapshot, notices,
  SBOM, and the automated dependency audit.
- Android has a Kotlin-to-JNI-to-Oboe entry for arm64 and x86_64. Its callback
  renders float PCM directly and publishes disconnection diagnostics without a
  JVM callback or lifecycle work on the realtime thread.
- Apple has an Objective-C++ AudioUnit entry with AVAudioSession playback setup
  and interruption, route-change, and media-services-reset handling. PCM stays
  inside the native render callback.
- HarmonyOS has an ArkTS-to-Node-API-to-OHAudio entry. Node-API transfers only
  lifecycle, bounded commands, and status; the OHAudio callback renders directly
  through the shared runtime and reports underflow/route/interruption diagnostics.
- Android and Harmony native sources compile in both MSVC configurations under
  warnings-as-errors using declaration-only platform header subsets. Apple is
  source-reviewed because Objective-C++ requires an unavailable Apple SDK.
- The M1 gate is complete: the same C4 sequence measures 261.25 Hz in Native,
  Wasm, and the ESP32 32 kHz core path, all analyzed samples are finite, the
  callbacks are allocation-free, and every required platform call entry exists.
- The M2 gate is complete: the default 30-key HID map, octave/transpose, all 8
  scales and 12 tonics, all 10 chord modes, continuous sustain, gesture-owned
  releases, sample-accurate commands, monophonic linear portamento, unified
  transport, accented metronome, six arpeggiator modes and six rates, including
  seeded deterministic random selection, are implemented in the portable core.
- Native and WebAssembly produce the same checked-in 35-event conformance digest
  (`83658a826364c67e`), transport frame, and final zero-voice/zero-gesture state.
  The test never rewrites its golden file.
- Property tests require sample-for-sample and event-for-event equality across
  unrelated render block sizes, run 2,000 deterministic random legal operations
  without NaN/Inf, and prove eventual sound/gesture cleanup.
- Absolute rational transport tests cover every quarter-note step through two
  hours at 48 kHz/120 BPM and reach frame 345,600,000 with zero cumulative
  rounding drift.

## In-progress work

- M3 DSP modules, 18 data-driven instruments, Patch compiler, Chorus, Delay,
  Reverb, master mixer/limiter, loudness calibration, golden audio, and automated
  audio analysis.

## Blocked platform checks

- Android NDK, Apple SDKs, and DevEco/HarmonyOS SDKs have not been discovered or
  verified. An official Android NDK download was attempted but the endpoint was
  unreachable from this host.
- Physical devices and signing credentials have not been provided.

## Exact validation commands

Run from a Visual Studio 2026 x64 developer shell with Ninja 1.13.2 on `PATH`:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release
build/dev-release/apps/mol-render/mol-render --output build/dev-release/c4-evidence.wav --duration 2 --sample-rate 48000 --channels 2 --note 60 --velocity 0.8
build/dev-release/apps/mol-play/mol-play --list-devices
build/dev-release/apps/mol-play/mol-play --duration 1.05 --note 60 --velocity 0.25
```

The evidence render was 384,044 bytes with peak 0.19609803, RMS 0.06978670,
zero clipped samples, zero non-finite samples, and zero underruns.
Both native configurations pass 17/17 CTest tests, including dependency audit,
M2 unit/property/event conformance, device enumeration, and the one-second
null-backend realtime callback test. They also compile the Android and Harmony
native entries under warnings-as-errors.

With the pinned Emscripten SDK environment active:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
```

Both configurations passed 15/15 tests on 2026-09-02. One test audits the locked
dependencies and license snapshot. M2 tests include the same exact event golden
used by Native and deterministic block-size/property checks. The worklet
conformance test loads the single-file worklet artifact in a mocked worklet
global and verifies stereo C4, finite output, message-driven note control, and
release to silence. The browser smoke page additionally passed in the Codex
in-app Chromium browser with `frequency=261.2500` and `peak=0.24512254`.

With the pinned ESP-IDF environment active:

```powershell
cd platforms/esp32
idf.py -B build-esp32 set-target esp32
idf.py -B build-esp32 build
idf.py -B build-esp32s3 set-target esp32s3
idf.py -B build-esp32s3 build
```

Both targets built successfully with ESP-IDF 6.1 and GNU 15.2.0 on 2026-09-02.
The build-local `sdkconfig` files allow the two configurations to coexist.
`idf.py size-components` verified the mapped core and host sizes recorded
above. Physical I2S playback and sustained-run counters remain unverified.

## Known failures

- `ninja`, `cl`, `emcc`, and `idf.py` are not on the default `PATH`; each is
  activated through its documented toolchain environment.
- Visual Studio 2026 Developer Command Prompt 18.8.0 provides MSVC 19.51.36248
  and Ninja 1.13.2; both Debug and Release validation succeeded on 2026-09-02.
- Apple source compilation, Android NDK builds, and HarmonyOS builds cannot run
  on the currently available host; their status documents do not claim otherwise.

## Next highest-priority task

Implement M3's complete DSP graph and 18 data-driven instruments, beginning with
the oscillator/envelope/filter building blocks and Patch schema/compiler.
