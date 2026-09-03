# Platform Porting Guide

## Port boundary

A new platform integrates the public C API; it does not fork the engine. The
host owns the audio device, monotonic clock, thread policy, permissions, route
changes, input APIs, persistence, package lifecycle, and user-facing UI. The
core owns all music mapping, transport, synthesis, effects, and canonical
recording behavior.

Start with a headless or minimal native smoke path. UI work begins only after
the same C4 command renders finite output through the real platform callback.

## Build the core

Compile the sources used by the `mol_core` CMake target as ISO C11 with the
platform's warnings-as-errors policy. Do not define platform macros to select
alternate music or DSP algorithms. Select exactly one resource profile and the
effect feature switches appropriate to the target.

Before adding device code, prove:

1. `mol_engine_config_default` is valid for the target profile.
2. Aligned caller storage satisfies `mol_engine_query_memory`.
3. A note-on, render, note-off sequence is finite and near C4.
4. The C consumer, shared sequence fixture, and all portable unit tests pass.
5. The link map and static memory fit the target budget.

## Desktop architecture cross-builds

The ordinary native presets cover the host architecture. Two additional
presets ensure that every headless desktop deliverable also compiles for the
required secondary architectures:

```sh
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
cmake --preset ci-linux-aarch64
cmake --build --preset ci-linux-aarch64
```

`ci-linux-aarch64` uses the system GNU cross compiler by default. A locally
unpacked Debian/Ubuntu cross sysroot can instead be selected with
`MOL_LINUX_AARCH64_ROOT`; keep its host-side shared libraries discoverable when
running its compiler binaries.

```powershell
$env:MOL_LLVM_MINGW_ROOT = "C:\path\to\llvm-mingw-ucrt-x86_64"
cmake --preset ci-windows-arm64-cross
cmake --build --preset ci-windows-arm64-cross
```

The Windows preset expects an LLVM-MinGW UCRT distribution and produces
COFF-ARM64 output. Both presets build the core, daemon, CLI, realtime playback,
sequence, Patch, renderer, and analyzer targets. Inspecting the object format
proves the target architecture; it does not prove runtime behavior. Native
ARM64 CI or a physical host must execute the regular test and device matrix
before promotion to `runtime-verified`.

## Audio host

Request float output when native support exists; otherwise convert from the
core's float PCM into a preallocated platform buffer. Pass the actual sample
rate and channel count to the engine. Do not assume the requested buffer size,
route, latency, or low-latency mode was granted—record the effective values.

The callback owns one engine and performs only bounded queue drain, render, and
format conversion. Device enumeration, opening, focus/session setup, recovery,
and logging run on a control thread. A route or media-service reset stops the
old callback before rebuilding its device object and restores the validated
persistent music state.

## Input and scheduling

Map physical keys by stable scan/usage code rather than localized text. Touch,
MIDI, GPIO, remote, and programmatic sources each receive a stable source ID;
each note gesture receives a nonzero 64-bit gesture ID. A matching note-off
uses the same gesture rather than recomputing the current scale or chord.

Translate trustworthy platform timestamps to absolute engine frames. When that
mapping is unavailable, use `MOL_FRAME_IMMEDIATE`; do not invent sub-buffer
accuracy. Producers use the host's bounded queue and handle queue-full results.

## Lifecycle and persistence

Implement explicit states for stopped, starting, running, interrupted,
recovering, and failed. Loss of focus or output must release or silence owned
gestures. Background audio is enabled only under the platform's documented
media-playback rules and only while playback or a user-visible musical task is
active.

Settings and recordings go to the platform's private/user data directory.
Validate before use, write a sibling temporary file, flush where available,
then atomically replace. Corruption falls back safely and remains diagnosable.
Never persist WebSocket tokens or upload input, recordings, device identifiers,
or child data by default.

## Capabilities

Build capability flags describe compiled core features. Host capability
responses add actual device, input, background, Web, radio, and low-latency
facts. A feature must disappear or return unsupported when hardware or the SDK
cannot provide it. In particular, OS-managed Bluetooth audio is an output
route, not an application A2DP implementation.

## Verification ladder

- Source-present: implementation and fail-closed build entry exist.
- Build-verified: an official target toolchain produced the package/firmware.
- Runtime-verified: it started and exercised the real native audio path.
- Device-verified: specified physical hardware, route, duration, and capture
  checks passed.

Record exact OS, SDK, compiler, hardware, route, sample rate, callback size,
latency measurement method, duration, underrun/xrun count, and artifact hash.
Do not promote one level using source review, an emulator, or another platform's
Web engine.

## Port completion checklist

- The official SDK builds from a clean checkout with pinned dependencies.
- The real callback uses the shared core and satisfies realtime rules.
- Interruption, route change, suspend/resume, and device-loss recovery pass.
- Foreground and legal background behavior match platform policy.
- Physical inputs use the unified event model and release on lifecycle loss.
- Private storage round-trips valid data and survives corrupt data.
- Capabilities and limitations match observed facts.
- Unit, sanitizer where available, long-run, latency, and package tests have
  evidence; unavailable checks remain explicitly open.
