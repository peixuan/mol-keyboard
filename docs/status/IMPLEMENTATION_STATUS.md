# Implementation Status

## Current milestone

M4 recording, playback, and offline tools is the highest unmet gate. M0 through
M3 are complete.

## Last verified commit

`40be63a` (`test(fuzz): exercise patch parsers with sanitizers`) is the last
verified code commit. The validation below was run on 2026-09-02 after that
commit. Documentation changes do not alter binaries.

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
  runtime; unavailable SDK/device validation remains explicitly unclaimed.
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
- `mol-render` writes deterministic PCM16 mono/stereo WAV with explicit preset,
  note, velocity, sample-rate, duration, and gate controls. `mol-audio-analyze`
  checks bounded RIFF/WAVE input and reports the required audio diagnostics.
- The Patch parser has a Clang libFuzzer entry covering arbitrary JSON and
  binary input plus successful-parse round trips. ASan/UBSan builds all core
  tests and the fuzzer; the Windows ASan runtime is deployed by CMake.
- ESP-IDF 6.1 currently builds the complete Tiny core, effects, and all 18
  compiled Patches for ESP32 and ESP32-S3. The current app binaries are 146,704
  and 172,560 bytes respectively, and the core archives are 20,422 and 20,646
  bytes. Detailed M3 evidence is in `docs/audio/M3_AUDIO_EVIDENCE.md`.

## In-progress work

- M4: event recording, deterministic playback, Mol Sequence Format v1,
  Standard MIDI File conversion, offline render integration, parser fuzzing,
  round-trip/truncation/golden tests, and long-sequence streaming.

## Blocked platform checks

- Apple SDKs and DevEco/HarmonyOS SDKs are not available on this Windows host.
- Physical Android/Apple/Harmony/ESP32 devices, audio hardware, signing
  credentials, and long-run device time are not available. No device-verified
  status is claimed.

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

MSVC 19.51.36248 passed 24/24 tests in Debug and LTO Release. A separate Tiny
profile passed 21/21; a Standard build with Chorus, Delay, and Reverb all
disabled passed 21/21.

With the pinned Emscripten environment active:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug --output-on-failure
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release --output-on-failure
```

Emscripten 6.0.5 and Node.js 22.16.0 passed 20/20 tests in Debug and LTO
MinSizeRel. Both configurations match the Native event golden and 18-preset
audio-metric golden tolerances.

For sanitizer and Patch fuzz validation, activate the Visual Studio environment
and place Clang 22 on `PATH`:

```powershell
cmake --preset fuzz-clang
cmake --build --preset fuzz-clang
ctest --preset fuzz-clang --output-on-failure
```

The ASan/UBSan configuration passed 19/19 tests. Its Patch libFuzzer smoke ran
for 20 seconds with the bounded JSON and compiled binary seeds and produced no
finding.

With the pinned ESP-IDF environment active, from `platforms/esp32`:

```powershell
idf.py -B build-esp32 build
idf.py -B build-esp32 size-components
idf.py -B build-esp32s3 build
idf.py -B build-esp32s3 size-components
```

Both ESP-IDF 6.1/GNU 15.2.0 targets rebuilt successfully. ESP32 uses 147,164 of
180,736 bytes reported internal DRAM (33,572 free); ESP32-S3 uses 172,655 of
341,760 bytes reported internal memory (169,105 free). These are build/map
results, not physical playback results.

## Known environment constraints

- `cl`, Emscripten, ESP-IDF, and the Clang ASan runtime are activated through
  their toolchain environments and are not all on the default `PATH`.
- Cross-platform source checks are not promoted to device verification.

## Next highest-priority task

Implement M4 beginning with the versioned, bounded `.molseq` parser/writer and
deterministic record/playback round trip, then connect MIDI conversion and the
offline tools to that format.
