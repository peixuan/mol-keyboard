# Implementation Status

## Current milestone

M0 — Blank repository and engineering baseline.

## Last verified commit

`894c1a1` (`feat(core): add caller-owned engine lifecycle`) is the last verified
commit. The M1 synthesis work below was verified in the current worktree and
will be committed atomically.

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

## In-progress work

- Offline WAV rendering of the verified native synthesis path.

## Blocked platform checks

- Emscripten is not installed on the current host; Wasm is not verified.
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
```

## Known failures

- `ninja`, `cl`, `emcc`, and `idf.py` are not on the default `PATH`.
- Visual Studio 2026 Developer Command Prompt 18.8.0 provides MSVC 19.51.36248
  and Ninja 1.13.2; both Debug and Release validation succeeded on 2026-09-02.

## Next highest-priority task

Add `mol-render` with a deterministic PCM WAV sink and validate its file output,
then compile the same synthesis source with Emscripten.
