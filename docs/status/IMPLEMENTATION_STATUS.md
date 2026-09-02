# Implementation Status

## Current milestone

M5 desktop headless product is the highest unmet gate. M0 through M4 are
complete.

## Last verified commit

`8db93a9` (`test(sequence): verify shared fixture across targets`) is the last
verified code commit. The validation below was run on 2026-09-03 after that
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
- Patch and Mol Sequence parser/writer Clang libFuzzer entries cover arbitrary
  input and successful-parse round trips. ASan/UBSan builds all portable tests
  and both fuzzers; the Windows ASan runtime is deployed for all test binaries.
- Native and Wasm parse the exact same generated 12-event sequence fixture and
  match one golden summary. The ESP32 and ESP32-S3 startup paths parse those
  same checked-in bytes before I2S starts; both firmware targets compile. The
  current app binaries are 153,440 and 179,328 bytes respectively. Detailed M4
  evidence is in `docs/sequence/M4_SEQUENCE_EVIDENCE.md`.

## In-progress work

- M5: implement the foreground-by-default `mol-keyboardd` service and `molctl`
  control client, including bounded JSON-RPC, authentication policy, observable
  health/state, desktop input/output integration, and clean-checkout evidence.

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

MSVC 19.51.36248 passed 51/51 tests in Debug and LTO Release. A separate Tiny
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

Emscripten 6.0.5 and Node.js 22.16.0 passed 23/23 tests in Debug and LTO
MinSizeRel. Both configurations match the Native event, sequence-fixture, and
18-preset audio-metric goldens.

For sanitizer and Patch fuzz validation, activate the Visual Studio environment
and place Clang 22 on `PATH`:

```powershell
cmake --preset fuzz-clang
cmake --build --preset fuzz-clang
ctest --preset fuzz-clang --output-on-failure
```

The ASan/UBSan configuration passed 23/23 tests. Its Patch and Mol Sequence
libFuzzer smoke sessions each ran for 20 seconds with valid corpus seeds and
produced no finding.

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

## Next highest-priority task

Implement M5 beginning with the bounded transport-neutral JSON-RPC dispatcher,
then connect `mol-keyboardd` and `molctl` through a local authenticated control
transport.
