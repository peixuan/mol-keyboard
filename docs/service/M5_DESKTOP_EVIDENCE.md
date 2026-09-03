# M5 Desktop Headless Evidence

Verified on 2026-09-03 at commit `c87e1a1`; the desktop-first regression and
device-free acceptance were refreshed through code candidate `b255bef`.

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
- On a WSL host with a running systemd user manager, the shipped hardened unit
  policy is transformed only for temporary executable/state paths, validated by
  `systemd-analyze`, linked into the runtime user-unit directory, and started by
  the real systemd manager. The gate requires clean process status and removes
  the runtime unit link before succeeding.
- Windows creates an actual WScript `.lnk` in an isolated Startup-directory
  model, validates its target, arguments, working directory, and hidden-window
  policy, launches the real daemon through that shortcut, and removes it with
  the production uninstaller. The real user Startup folder is not modified.
- The macOS CTest lane now bootstraps a temporary user LaunchAgent from the
  shipped property-list template. It runs the exact built daemon with a null
  sink, drives the exact built CLI through status, capability, preset, tempo,
  note, recording, playback, self-test, doctor, benchmark, all-notes-off, and
  shutdown paths, and requires launchd exit code zero plus socket cleanup.
- Linux CI runs that same production acceptance script unchanged against a
  controlled `launchctl`, `plutil`, and Darwin-host model. The model starts the
  real Linux daemon and CLI as a supervised process group, enforces the shipped
  plist lifecycle and exact executable contract, records bootstrap/bootout,
  and rejects a nonzero service exit or surviving supervisor.

## Verification results

| Configuration | Result | Relevant evidence |
|---|---:|---|
| Windows MSVC Debug | 78/78 | local IPC recovery, all 41 RPC methods, runtime callback, independent daemon process, CLI, recording/playback, rendering, macOS interface simulations |
| Windows MSVC LTO Release | 86/86 | current optimized suite plus real temporary Startup-shortcut product lifecycle, portable service-asset audits, and iOS hardware-key ownership simulation |
| Linux x86_64 GCC (WSL) | 87/87 | current Unix socket/null-audio product suite, real systemd user-service lifecycle, executable macOS LaunchAgent orchestration simulation, portable Windows service audit, and iOS hardware-key ownership simulation |
| Linux x86_64 Clang (WSL) | 78/78 | Unix socket mode/cleanup, null-audio service process, CLI lifecycle, Linux adapter compilation, macOS interface simulations |
| Linux AArch64 QEMU 10.2.1 | 59/59 | target core/DSP/music tests, 18-preset metrics, null playback, nested daemon process, CLI/render lifecycle |
| Windows Clang ASan/UBSan | 30/30 | all sanitizer-enabled portable/control tests and four 20-second parser fuzz sessions |
| Emscripten MinSizeRel | 38/38 | current core/worklet regression plus platform acceptance-project audits and iOS hardware-key ownership simulation |
| ESP32 / ESP32-S3 | build passed | firmware regression; application binaries remain 153,440 and 179,328 bytes |

The production Web/PWA application was also run against the current desktop
service rather than only as a standalone synthesizer. System Chrome on Windows
and bundled Chromium 151.0.7922.34 on Linux each passed five applicable desktop
application tests, with two capability-specific cases skipped. The service
controller test spawned the platform's real `mol-keyboardd`, authenticated over
the loopback WebSocket, delivered keyboard events, recorded a sequence, rejected
an invalid token, shut down over local IPC, and observed a clean process exit.

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
host were unavailable. The exact production `physical_input_macos.cpp` now
compiles and executes against a controlled IOHID model under both MSVC and Linux
Clang, covering enumeration, press/repeat/release, gesture ownership, and detach
cleanup. The exact production `audio_runtime.cpp` also executes with a
controlled CoreAudio/miniaudio model, covering backend selection, effective
stream configuration, callbacks, device selection, reroute/stopped
notifications, recovery, and cleanup. A new fail-closed runner now gives the
macOS CI lane a real LaunchAgent daemon/CLI process acceptance path without
requiring audio hardware. The unchanged runner additionally passes on Linux
against a controlled launchd process model while starting the real daemon and
CLI, which validates orchestration, product behavior, zero-exit shutdown, and
cleanup without claiming Apple implementation behavior. These are still
explicitly unexecuted Apple results on this host: native launchd, Apple
framework ABI, CoreAudio devices, IOHID permissions, and the native macOS
daemon process require a real macOS run before promotion.

The later `d45383b` release audit also cross-built the complete desktop product
with checked-in presets. GNU 15.2.0 produced AArch64 ELF daemon, CLI, playback,
sequence, render, patch, analyzer, and core outputs. LLVM-MinGW 20260826/Clang
23.1.0 produced the equivalent COFF-ARM64 outputs. These results promote both
architectures to `build-verified`; they are not native ARM64 runtime evidence.

At candidate `b3b7e14`, QEMU user-mode emulation additionally executed the
Release AArch64 daemon and CLI over a private Unix socket. Record/playback,
doctor, self-test, a 96,000-frame finite benchmark, all-sound-off, persistent
configuration, and clean shutdown passed; the AArch64 renderer produced a
finite, non-silent 4.25-second WAV with no clipping or underruns. A separate
Debug cross-test build passed 59/59 tests. The JSON report labels this
`simulated-runtime` and excludes native scheduling, physical audio/input,
latency, route change, suspend, and device-loss claims.

At candidate `5b0640b`, `platforms/macos/run-launchd-smoke.sh` was registered as
an Apple-only CTest with a 30-second fail-closed timeout. A portable project
audit proves that the shipped LaunchAgent template, Apple-only registration,
macOS CI runner, null-audio process launch, complete CLI lifecycle, diagnostic
assertions, zero-exit check, and success marker remain connected. That audit
passes under MSVC, Linux GCC, and Emscripten; the actual LaunchAgent CTest has
not run on this Windows host and is not recorded as macOS runtime evidence.

At candidate `a9b8e98`, Linux GCC executes that production runner through a
fail-closed launchd model. The model accepted only the expected built daemon,
then the real `mol-keyboardd` and `molctl` completed null-audio startup, all
control/record/play/diagnostic assertions, a finite 4,096-frame benchmark,
clean shutdown, socket removal, and both bootstrap and bootout. This raised that
candidate's Linux suite to 83/83. The runner also exposed and fixed a case-sensitive
null-backend assertion (`Null` is miniaudio's real backend name). This is
device-free service-orchestration evidence, not native macOS evidence.

At candidate `938f955`, the WSL systemd 259 user manager validated, linked, and
started a unique runtime unit retaining the shipped restart and sandbox policy.
The real daemon and CLI completed the same null-audio control, recording,
playback, diagnostic, benchmark, and shutdown checks. Systemd reported
`ActiveState=inactive`, `Result=success`, and `ExecMainStatus=0`; the socket and
runtime unit link were gone before the test passed. Environments without a
systemd user manager report an explicit CTest skip instead of imitating one.

At candidate `b255bef`, Windows created and inspected a real shell shortcut in
a unique temporary directory, launched the exact Release daemon through it with
a hidden window and private Named Pipe/state, and drove the real CLI through
control, recording, playback, diagnostics, a finite 4,096-frame benchmark, and
shutdown. The retained process handle reported exit code zero; the production
uninstaller removed the shortcut and the runner removed all temporary state.
The installer/uninstaller still target the real current-user Startup folder by
default; their directory override exists for isolated acceptance only.
