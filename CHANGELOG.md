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
