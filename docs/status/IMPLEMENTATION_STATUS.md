# Implementation Status

## Current milestone

M0 portability baseline and M1 portable sound path.

## Last verified commit

`be59267` (`feat(render): add deterministic PCM WAV output`) is the last verified
commit. The WebAssembly implementation below was verified in the current
worktree and will be committed atomically.

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

## In-progress work

- ESP-IDF component integration and a real ESP32/ESP32-S3 toolchain build.

## Blocked platform checks

- ESP-IDF is not installed on the current host; ESP32 builds are not verified.
- Android NDK, Apple SDKs, and HarmonyOS SDKs have not been discovered or verified.
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
```

The evidence render was 384,044 bytes with peak 0.19609803, RMS 0.06978670,
zero clipped samples, zero non-finite samples, and zero underruns.

With the pinned Emscripten SDK environment active:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
```

Both configurations passed 5/5 tests on 2026-09-02.

## Known failures

- `ninja`, `cl`, `emcc`, and `idf.py` are not on the default `PATH`.
- Visual Studio 2026 Developer Command Prompt 18.8.0 provides MSVC 19.51.36248
  and Ninja 1.13.2; both Debug and Release validation succeeded on 2026-09-02.

## Next highest-priority task

Provision the current stable ESP-IDF toolchain, add `mol_core` as a component,
and perform real ESP32 and ESP32-S3 compile checks before I2S integration.
