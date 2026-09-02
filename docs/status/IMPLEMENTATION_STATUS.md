# Implementation Status

## Current milestone

M0 — Blank repository and engineering baseline.

## Last verified commit

None. The first native build has not yet been committed.

## Completed requirements

- Project identity, clean-room statement, Apache-2.0 license, privacy policy,
  and bilingual introductory documentation are present.
- The required development branch is `codex/mol-keyboard-v1`.

## In-progress work

- Portable C11 engine lifecycle, CMake/Ninja presets, and C/C++ consumer tests.

## Blocked platform checks

- Emscripten is not installed on the current host; Wasm is not verified.
- ESP-IDF is not installed on the current host; ESP32 builds are not verified.
- Android NDK, Apple SDKs, and HarmonyOS SDKs have not been discovered or verified.
- Physical devices and signing credentials have not been provided.

## Exact validation commands

No implementation validation has completed yet. The next native validation is:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

## Known failures

- `ninja`, `emcc`, and `idf.py` are not on the default `PATH`.
- No C or C++ compiler is on the default `PATH`; a Visual Studio 2026 developer
  environment is available and will be used for native validation.

## Next highest-priority task

Complete and verify the caller-provided-memory engine lifecycle with C and C++
consumers, then add the offline WAV sound path.
