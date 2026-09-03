# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added

- Established the clean-room MoL Keyboard project identity and Apache-2.0 licensing.
- Added bounded Mol Sequence v1 recording/playback, strict JSON and MIDI conversion,
  non-destructive sequence editing, deterministic multi-format offline rendering,
  parser/writer fuzzing, and shared Native/Wasm/ESP32 fixture conformance.
- Added the `mol-keyboardd` desktop service and `molctl` client with bounded
  local IPC and JSON-RPC, realtime miniaudio output, native keyboard adapters,
  hotplug recovery, atomic configuration, diagnostics, and user startup assets.
- Added bounded realtime MIDI 1.0 input with running-status parsing, channel
  filtering, Note/velocity/pressure, CC1 modulation, CC64 sustain, pitch bend,
  program changes, panic controls, and native WinMM, Linux raw-MIDI, and
  CoreMIDI adapters.
- Added sanitizer-backed fuzzing for all six required Patch, Sequence, service
  configuration, JSON-RPC, Wire, and MIDI-file parser boundaries, plus realtime
  MIDI streams, the latency capture, WAV analyzer, ESP32 settings/Web-form, and
  HID report parsers.
- Added the complete installable offline Web/PWA instrument with paired
  AudioWorklet/Wasm loading, MessagePort and SharedArrayBuffer transports,
  IndexedDB persistence, keyboard/touch control, authenticated service mode,
  and current-browser automation.
- Added Android Oboe, iOS AudioUnit/AVAudioSession, and HarmonyOS OHAudio
  application hosts with strict native bridges, legal background lifecycles,
  private recording storage, platform packaging, and truthful capability
  boundaries.
- Added build-verified ESP32 and ESP32-S3 firmware with I2S, GPIO matrix, HID,
  persistent settings/sequences, physical recovery, optional local Web
  configuration, target-specific A2DP/USB support, and fail-closed HIL tooling.
- Added release coverage, static-analysis, sanitizer/fuzz, ThreadSanitizer,
  endurance, size, license/SBOM, extracted-package headless runtime, and
  clean-checkout gates with evidence. External device, Apple/Harmony, Safari,
  and physical latency gates remain required before v1.0.0.
- Added reproducible Windows ARM64 and Linux AArch64 cross-build targets for the
  complete headless product, plus native ARM64 CI runners.
- Added explicit static/shared `mol_core` builds, a hidden-by-default 47-symbol
  public export boundary, isolated installed-package consumers, independent
  C11/C++17 compilation for every public header, and Linux ABI
  symbol/layout/signature compatibility checks against the version 1.0 baseline.
- Added `mol-latency-probe` for fail-closed analysis of multichannel PCM16
  physical captures, with report metadata, capture hashes, raw observations,
  P50/P95/maximum statistics, acceptance thresholds, and malformed-input fuzzing.
- Added explicit Release+LTO Tiny, Standard, and Full resource-profile presets
  and CI coverage, including Full-profile host storage and recording capacity.

### Fixed

- Disabled strict-alias optimization for the C core's documented caller-owned
  opaque storage contract, preventing cross-language GCC LTO miscompilation.
