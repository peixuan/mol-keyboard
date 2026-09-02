# Implementation Status

## Current milestone

M0 — Blank repository and engineering baseline.

## Last verified commit

`8d62f8f` (`chore: establish MoL Keyboard project baseline`) is the last
previous commit. The lifecycle implementation below was verified in the current
worktree and will be committed atomically.

## Completed requirements

- Project identity, clean-room statement, Apache-2.0 license, privacy policy,
  and bilingual introductory documentation are present.
- The required development branch is `codex/mol-keyboard-v1`.
- `mol_core` is ISO C11 and initializes entirely in aligned caller-owned memory.
- Debug and release builds pass warnings-as-errors with MSVC 19.51.36248.
- C and C++17 consumers link and run against the same public C ABI.
- Interleaved and planar no-op rendering is allocation-free and advances frame time.

## In-progress work

- Offline WAV rendering and the first band-limited oscillator/ADSR sound path.

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

Add an offline WAV sink and a real oscillator/ADSR/voice path, then verify a C4
render by frequency and finite-sample analysis.
