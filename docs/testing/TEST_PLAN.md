# Test Plan

## Principles

Tests prove one shared engine before platform presentation. A source review,
emulator, synthetic render, or parser fixture is never relabeled as physical
device evidence. Every gate fails closed: an unavailable required tool or
target remains open instead of being skipped and reported as passed.

Release evidence records commit, date, toolchain, host/target, exact command,
result, artifact hash where applicable, and any unverified boundary. Generated
build directories, credentials, raw user/device logs, and absolute local paths
are not committed.

## Fast native regression

The default Debug and optimized Release presets compile with warnings as
errors and run all applicable unit, integration, format, tool, service, source
audit, and consumer tests. Node.js must be on `PATH`, or `EMSDK_NODE` must name
its executable, so the exact production HarmonyOS policy test cannot disappear
from a native suite. Python 3 is also required so the HIL/QEMU evidence parsers
and Linux launchd-process simulation cannot disappear:

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release
```

Core cases cover lifecycle, invalid arguments, memory bounds, scheduling,
queue overflow, music transformations, gesture ownership, stuck-note recovery,
voice stealing, oscillator/filter/envelope/effects behavior, all 18 presets,
recording/playback, transport, and randomized invariants. A dedicated native
gate installs the SDK into an empty prefix with CMake package registries disabled,
requires the installed package configuration, compiles each of the 14 public
headers independently in both C11 and C++17 modes, and runs standalone C and C++
consumers against the installed static or shared library.

## Cross-target conformance

Emscripten Debug and MinSizeRel run the portable suite under pinned Node. Native
and Wasm must match the event trace, shared sequence fixture, and calibrated
18-preset audio metrics. ESP-IDF builds the same fixture into ESP32 and
ESP32-S3 startup checks.

The `ci-linux-aarch64` and `ci-windows-arm64-cross` presets must compile the
complete headless product, not only `mol_core`, and object inspection must
confirm AArch64 ELF and COFF-ARM64 outputs. CI additionally runs the ordinary
native suite on Ubuntu and Windows ARM64 hosted runners; cross-build success is
never promoted to runtime verification.

Block-size conformance renders equivalent streams with different callback
partitions. The two-hour rational transport case must end at the exact expected
frame with no accumulated drift.

## Parser and memory safety

`fuzz-clang` enables ASan, UBSan, and eleven libFuzzer targets for Patch,
Sequence, service configuration, JSON-RPC, MolWireEventV1, MIDI, latency and
audio WAV captures, ESP32 settings and Web forms, and HID report parsing. Each
CI smoke session runs at least 20 seconds and reparses/canonicalizes successful
inputs where applicable. Reproducer artifacts are reviewed privately before
disclosure.

```sh
cmake --preset fuzz-clang
cmake --build --preset fuzz-clang
ctest --preset fuzz-clang
```

## Resource profiles

All three compile-time resource profiles have dedicated optimized presets and
run the complete applicable suite with LTO. Standard includes the calibrated
audio-metric golden; Tiny and Full omit that deliberately Standard-specific
comparison while retaining all other conformance, service, and tool tests.

```sh
cmake --preset profile-tiny
cmake --build --preset profile-tiny
ctest --preset profile-tiny
cmake --preset profile-standard
cmake --build --preset profile-standard
ctest --preset profile-standard
cmake --preset profile-full
cmake --build --preset profile-full
ctest --preset profile-full
```

ThreadSanitizer runs the Linux concurrent service, queue, IPC, and process
paths through `tsan-clang`. Static analysis applies the checked-in Clang-Tidy
policy as errors to every first-party production translation unit.

## Coverage

The Clang source-based coverage gate measures first-party `mol_core` code, not
tests or generated Patch arrays. It requires at least 90% overall line coverage
and 95% in queue/memory, music/state, Patch, and Sequence critical modules.
`tools/coverage_gate.py` rejects missing objects, tools, sources, or thresholds.

## Audio and endurance

Audio goldens are read-only fixtures. Updates require an explicit analyzer run,
metric review, listening rationale, and manual fixture change. All outputs must
be finite, bounded, non-silent where expected, and within documented peak, RMS,
DC, timing, step, stereo, and spectral tolerances.

The opt-in Release endurance label renders 30 simulated minutes with 32 voices,
rapid notes, every preset, recording cycles, and near-full queue pressure. It
must remain at least four times realtime. A separate test rebuilds an actual
null device 30 times. These tests catch long-state and recovery defects but do
not prove physical callback deadlines or routes.

## Web and mobile

Node tests cover UI/controller logic. Playwright runs the production bundle and
real AudioWorklet/Wasm path for supported desktop browsers, mobile layouts,
autoplay, keyboard/touch ownership, MessagePort and SharedArrayBuffer paths,
suspend/resume, blur/visibility release, recording, service control, install,
and offline reload. Current Safari acceptance requires Safari on Apple hardware.

Android builds both required ABIs, Debug, unsigned Release, instrumentation,
R8/lintVital, and full lint. Device acceptance additionally covers a physical
low-latency path, hardware keyboard, wired/Bluetooth routes, background and
screen-off lifecycle, interruption, persistence, and measured latency. Apple
and Harmony follow equivalent official-toolchain and physical-device matrices.

The Android emulator instrumentation injects transient focus loss/gain into
the production service listener and requires AAudio to stop, reopen, and resume
finite callbacks. This covers the restoration state machine; a competing app
and physical output routes remain device acceptance.

## ESP32 hardware-in-loop

Both chip families must build default and Web variants within flash and static
memory budgets. The release HIL uses physical UART, GPIO/HID input, and I2S
capture for 30 minutes. It rejects resets, watchdogs, deadline misses, queue or
persistence errors, missing target capabilities, silence, or clipped capture.
ESP32 additionally tests Classic A2DP; ESP32-S3 tests USB HID and confirms A2DP
absence. Parser self-tests alone are not device evidence.

The device-free `esp32_hil.py --simulate` gate runs 30 minutes of virtual
telemetry for both chip families and injects reset, deadline, stalled-audio,
and firmware-error failures. It verifies the HIL state machine and reporting,
is always labeled `simulated-hil`, and never satisfies the release HIL row.

The separate `run-qemu.ps1` gate builds and boots each real ESP-IDF image on
the corresponding Espressif QEMU machine. It requires NVS/FAT initialization,
the shared sequence and C4 checks, a synthetic note through the production
input queue, three FreeRTOS audio/control snapshots, non-silent finite PCM, and
zero project failure counters. Its configuration replaces I2S pacing with a
virtual sink and prevents GPIO, Bluetooth, A2DP, and USB startup. Reports are
always labeled `emulated-firmware`; QEMU performance and all physical, radio,
USB, I2S, acoustic, power, thermal, watchdog, and endurance claims are excluded.

## Performance, latency, size, and packages

The release size gate strips a copy of the native archive, gzip-compresses Wasm,
and measures every deployable Web resource except source maps. All limits are
exclusive. CPack produces checksummed ZIP/TGZ distributions whose contents,
CMake target, daemon version, and CLI help are audited after extraction.

End-to-end latency uses a physical stimulus and captured acoustic/electrical
response. Report P50/P95/max separately for built-in or wired, USB, OS Bluetooth,
Web Audio, ESP32 I2S, and ESP32 A2DP routes. Built-in/wired acceptance is P95 at
most 50 ms with a 30 ms target; supported desktop Web targets require P95 at
most 50 ms. Bluetooth has no fabricated 50 ms threshold.

Use `mol-latency-probe` and the fail-closed acquisition procedure in
`docs/testing/LATENCY_MEASUREMENT.md`. The committed analyzer tests generate a
deterministic synthetic recording, verify its statistics and threshold failure,
and reject corrupt containers. Those tests prove the measurement code only;
they never close a physical route row.

## Release decision

`v1.0.0` is permitted only when the status matrix links every mandatory build,
runtime, physical-device, browser, long-run, latency, dependency, license,
SBOM, package, documentation, and clean-checkout result. Any missing external
host, device, capture instrument, or signing requirement keeps the release gate
open even when all local automation passes.
