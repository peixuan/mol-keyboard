# M5 Desktop Headless Evidence

Verified on 2026-09-03 at commit `c87e1a1`.

## Implemented surface

- `mol-keyboardd` is a foreground-by-default ordinary-user service. Its fixed
  realtime command queue and caller-owned engine arena keep allocation,
  blocking, logging, and cross-language calls outside the audio callback.
- The desktop runtime uses miniaudio, follows the operating system's output
  devices, recovers from output hotplug outside the callback, and can fall back
  explicitly to a null sink for headless CI and diagnosis.
- Windows Raw Input, Linux evdev, and macOS IOHIDManager adapters map physical
  keys into the same gesture-owned command model. Detach and shutdown release
  every owned gesture.
- Local control uses a remote-rejecting Windows Named Pipe or a mode-0600 Unix
  domain socket. Frames are little-endian length-prefixed and limited to 64
  KiB. Client I/O is time-bounded, incomplete clients are isolated, and no LAN
  listener or browser UI starts by default.
- The strict bounded JSON-RPC dispatcher registers all 41 service methods.
  Configuration is schema-checked, limited to 64 KiB, atomically replaced, and
  restored on restart. Recordings remain leaf `.molseq` files in the private
  state directory.
- `molctl` covers status, capabilities, devices, input/output selection,
  presets, notes, sustain, tempo, chords, arpeggiator, recording, playback,
  rendering, all-notes-off, doctor, self-test, and benchmark. It provides human
  output and `--json` automation output.
- User-level startup assets are supplied for systemd, launchd, and the current
  Windows user's Startup folder. Shutdown sends all-sound-off before input,
  audio, IPC, and engine resources are released.

## Verification results

| Configuration | Result | Relevant evidence |
|---|---:|---|
| Windows MSVC Debug | 63/63 | local IPC recovery, all 41 RPC methods, runtime callback, independent daemon process, CLI, recording/playback, rendering |
| Windows MSVC LTO Release | 63/63 | same optimized suite; daemon plus CLI total 586,240 bytes |
| Linux x86_64 Clang (WSL) | 63/63 | Unix socket mode/cleanup, null-audio service process, CLI lifecycle, Linux adapter compilation |
| Windows Clang ASan/UBSan | 30/30 | all sanitizer-enabled portable/control tests and four 20-second parser fuzz sessions |
| Emscripten Debug/MinSizeRel | 24/24 each | core regression after the control-plane changes |
| ESP32 / ESP32-S3 | build passed | firmware regression; application binaries remain 153,440 and 179,328 bytes |

An independently started Windows Release daemon enumerated four WASAPI
outputs and the `raw-input:all-keyboards` physical adapter. `molctl doctor`
reported all 18 patches valid, writable private storage, compatible ABI,
working local IPC, healthy realtime counters, and the active 48 kHz stereo
WASAPI output. The offline benchmark rendered 96,000 frames with zero
non-finite samples at 80.68 times realtime, and RPC shutdown returned process
exit code 0.

The host exposed no Bluetooth output during this run, so `doctor` correctly
reported that fact and directed the user to system pairing. No Bluetooth
speaker playback claim is made. The Linux run used WSL and a null sink, so it
does not claim physical evdev or Linux audio hardware. An Apple SDK and macOS
host were unavailable; the IOHIDManager adapter and launchd asset are present
but macOS compilation/runtime acceptance remains unverified.
