# Implementation Status

## Current milestone

The desktop application and headless-service matrix remains the first priority,
followed by mobile and ESP32. The production Web UI now runs inside a native
wxWidgets frame backed by WebView2 on Windows, WebKit2GTK on Linux, and WKWebView
on macOS. Real Windows and Linux system-WebView acceptance verifies
AudioContext, AudioWorklet, cross-origin isolation, IndexedDB, and the supported
SharedArrayBuffer or MessagePort transport without exposing a native script
bridge. An additional fully native wxWidgets debugger drives the independent
daemon over bounded local IPC. The portable release preset now includes both
GUI applications, and package audit must launch the native debugger against an
independent packaged daemon before it runs the separate no-UI daemon/CLI
lifecycle. The GUI-enabled suites pass 98/98 on Windows and
100/100 on Linux; the daemon and CLI remain independently buildable and usable
with both GUI options disabled. A fresh Windows build with both GUI options and
the local Web server disabled compiles 172 targets and passes 93/93 tests,
including the independent daemon process lifecycle.
Windows now also creates, validates, launches, and uninstalls a real WScript
Startup shortcut in an isolated directory while exercising the real daemon and
CLI; the actual user Startup folder is untouched.
The Linux suite now also validates the shipped hardened unit with
`systemd-analyze` and runs the real daemon/CLI lifecycle under the actual
systemd 259 user manager, removing its runtime-only unit afterward.
The exact macOS IOHID and CoreAudio-selected production sources also pass
controlled lifecycle simulations under two non-Apple compilers; this does not
replace an Apple SDK or macOS runtime result. The Apple-only CTest bootstraps
the shipped LaunchAgent and exercises the real daemon/CLI lifecycle with null
audio. Linux now executes that exact runner unchanged against a controlled
launchd process model and the real product binaries, while the portable audit
keeps both paths fail-closed. Native Apple execution remains pending. A
dedicated macOS desktop job now builds the production
Web/Wasm payload and both GUI applications, exercises WKWebView and native IPC,
and audits the extracted GUI plus no-UI service lifecycle. Its portable wiring
audit passes on Windows and Linux; the Apple job itself remains unexecuted.
With those locally reachable desktop/service gates exhausted, both real ESP-IDF
firmware images now also execute through their production storage,
input/control, shared-core, and FreeRTOS audio paths in Espressif QEMU. Other locally actionable M10 release
gates are complete. Native and Wasm
regression, Release+LTO Tiny/Standard/Full profiles, static/shared ABI
verification, coverage, static analysis, ASan/UBSan with all twelve fuzzers,
Linux ThreadSanitizer, optimized endurance, release-size budgets,
dependency/license and SBOM audits, extracted Windows/Linux package WebView,
native-debugger, and independent headless lifecycles, Android packaging, and
clean checkout reproduction pass. Complete Windows ARM64 and Linux AArch64 desktop
products now cross-build through checked-in presets, closing their local build
gap. Linux AArch64 also passes an end-to-end QEMU product gate and 71/71 target
tests, while execution on native ARM64 hosts remains unclaimed. Android
emulator coverage now also dispatches all 30 production hardware-key mappings
before WebView handling, rejects repeat retriggers, and stops and reopens AAudio
across injected transient focus loss/gain. A fail-closed runner now owns APK
installation, result parsing, service-leak detection, and cleanup on both the
local API 35 headless emulator and the checked-in CI lane. The exact
background-policy state machine consumed by the iOS
controller now also executes under MSVC, Linux GCC, and Emscripten. Its exact
hardware-key mapping and ownership state machine does too, including repeat,
rollback, and deactivation release behavior. A checked-in
iPhone Simulator gate now installs and launches the packaged application and
requires both production-UI readiness and valid/rejected reply-bridge behavior;
it still requires a real Apple CI execution. The unchanged production
HarmonyOS Node-API bridge and OHAudio host now execute against controlled API
models across MSVC, Linux GCC, Emscripten, Clang sanitizers, and AArch64 QEMU.
The exact production HarmonyOS background-policy `.ets` source also executes
without transformation under Node.js, is consumed directly by `AudioService`,
and passes strict ArkTS compilation in Debug and Release compatibility HAPs.
The compatibility wrappers now clean Hvigor output before every assembly and
report the packaged ArkTS bytecode digest, preventing stale incremental
`modules.abc` output from being mistaken for a current package. M0 through M9 are
implementation-complete, but the Definition of Done is not complete: native
ARM64 runtime, Apple and Harmony toolchains, current Safari, physical mobile
devices, ESP32 hardware, physical audio routes, and instrumented end-to-end
latency remain external acceptance gates. No `v1.0.0` tag exists.

## Last verified commit

`0dbce9e` (`test(android): gate native keyboard in emulator`) is the latest
exact candidate reproduced from a clean local clone. It compiled all 108
Emscripten actions, passed 46/46 MinSizeRel tests, rebuilt and tested the
production Web bundle from 20 locked packages with zero vulnerabilities and
12/12 tests, and passed clean dual-ABI Android Debug, instrumentation, unsigned
Release, and lint builds. Its API 35 headless-emulator gate reported AAudio API
2 at 48 kHz, 30/30 production hardware-key mappings, repeat suppression,
focus-loss/reopen recovery, 101 background callbacks, 224 screen-off callbacks,
idle shutdown, no residual foreground service, and instrumentation code `-1`.
The exact Debug APK is 3,644,823 bytes with SHA-256
`26a188c57268b4ccaa4d117bfd869befe7ecd8fb274e5278a4b799dea84dc64f`;
the 2,590,764-byte unsigned Release APK has SHA-256
`6010ee48225b3b050b307b3b5ab4dac07ab5e681dabf91d6632a4bba05a2e96e`.

The preceding exact desktop-package candidate `6897843` enables both wxWidgets
applications, and a portable CMake audit prevents that contract from silently
regressing. Package audit requires and launches the extracted system-WebView
instrument and native service debugger on Windows, Linux, and macOS layouts.
The debugger must control and shut down an independent packaged null-audio
daemon over local IPC; audit then starts a fresh daemon for the complete no-UI
CLI lifecycle. Its exact clean Windows clone compiled 108 Emscripten targets,
passed 44/44 MinSizeRel tests, rebuilt the production Web bundle from 20 locked
packages with zero vulnerabilities, compiled 575 native Release+LTO actions,
and produced a 4,463,145-byte, 153-file ZIP with SHA-256
`5c4cfff1a1272f42828d7b911f72f54d1dee009c3da78589145800e4558efd34`.
Its WebView2 instrument reports `PASS-SharedArrayBuffer`, its packaged native
debugger passes state/capability/self-test/note/release/shutdown RPC, and the
subsequent headless lifecycle reports 48 kHz stereo, native MIDI, 4,096 finite
benchmark frames, and exit code zero. A fresh Linux build compiled 543 actions
and produced a 7,732,484-byte, 152-file TGZ with SHA-256
`10521374f9ebc222d507eb99138191190b371d879bc7138f849d6b9050b6f0ae`.
Its WebKitGTK instrument reports `PASS-MessagePort`, and the same native-debug
and no-UI lifecycle gates pass under Xvfb. Package audit schema 4 records all
three independent runtime paths. The preceding `3e63c2e` candidate adds the
macOS Release+LTO desktop CI and retains the full GUI-enabled regressions:
98/98 on Windows and 100/100 on Linux. The
preceding `23ff832` candidate includes a bounded streaming MIDI 1.0 decoder,
public CC1 modulation, native WinMM/Linux raw-MIDI/CoreMIDI adapters, explicit
Omni or Channel 1--16 filtering, truthful service capability/device reporting,
and feature-off support. The realtime decoder has a dedicated ASan/UBSan
libFuzzer target that checks arbitrary chunk-boundary equivalence and bounded
gesture cleanup. Windows LTO Release passes 95/95, Linux GCC passes
95/95, and Emscripten MinSizeRel passes 42/42. Linux runs the exact raw-MIDI
adapter through a kernel FIFO; the exact CoreMIDI source passes controlled API
execution under MSVC and GCC. No physical MIDI or native Apple claim is made.
The package audit requires all three
desktop service definitions, safely extracts the archive, and starts the
packaged daemon with no UI, a null audio backend, private state, and private IPC.
The packaged CLI verifies capabilities, preset and tempo changes, note input,
recording persistence, playback, latency status, self-test, doctor, a finite
4,096-frame benchmark, all-notes-off, zero-exit shutdown, and endpoint cleanup.
Fresh optimized Windows and Linux archives both pass this lifecycle locally
and must report native MIDI support;
the checked-in Linux release job and new Windows package job enforce it when
published. The new workflow path is YAML-validated but remains unexecuted for
this unpushed commit.
The preceding `e64e8f0` candidate adds the installed SDK consumer gate. Its exact
tree at documentation commit `cc3af78` was also reproduced from a new clone with
empty build trees on Windows, Linux, and Emscripten. Every ordinary native test configuration now installs
the SDK into an empty isolated prefix, disables CMake package registries,
requires exactly one installed `mol_keyboardConfig.cmake`, compiles every one of
the 14 public headers independently as C11 and C++17, and runs independent C11
and C++17 consumers linked through the installed `mol::core` target. Windows
LTO Release passes 91/91, Linux GCC passes 92/92, and Emscripten MinSizeRel
passes 42/42. GNU 15 Release+LTO Tiny, Standard, and Full pass 91/91, 92/92,
and 91/91. Windows and Linux shared builds pass 89/89 and 90/90; the Linux
library still matches all 47 public symbols, and ABI Compliance Checker reports
100% binary and source compatibility with no problems or warnings. Instrumented
and cross builds retain their dedicated gates without exporting development
runtime flags as part of the installed SDK contract.
The preceding `5c230cd` candidate makes test-enabled CMake configuration require
Python instead of silently omitting both ESP32 evidence-parser tests and the
Linux launchd-process simulation. An intentionally invalid interpreter fails
configuration; Python 3.14.6 on Windows and 3.14.4 on Linux configure
successfully.
The preceding `5e84b49` candidate makes browser acceptance fail when the real
desktop daemon is absent, and the checked-in Linux CI path builds that daemon
before running the locked production Web bundle across Chromium, Firefox, and
WebKit. The missing-daemon rejection and real Windows/Linux daemon paths
execute locally with the new portable wiring audit. The CI workflow itself
remains unexecuted for this unpushed commit. The preceding `00a0e50` candidate
makes native test configuration fail closed when Node.js is unavailable instead
of silently omitting the exact HarmonyOS production-policy test. The production
Web controller was rebuilt from locked dependencies and passed all five applicable
Chrome desktop cases against real Windows and Linux daemon processes at
repository candidate `4cdb929`. The preceding `098ddf4` candidate makes the
OpenHarmony compatibility wrappers run
a module clean before assembly and report the packaged `ets/modules.abc`
digest. A clean checkout of that commit rebuilt Windows and Wasm from empty
build trees and produced a fresh Release HAP. The preceding `b48c680` candidate
makes `AudioService` consume a single production `.ets` state machine for user
intent, foreground/background, playback, metronome/transport, continuous-task
retention, idle release, and route recovery. Node.js imports and executes that
exact source without transformation, while the OpenHarmony toolchain
type-checks and compiles it into both HAP variants. Windows, Linux,
Emscripten, targeted Clang audit, and AArch64 QEMU suites pass without claiming
HarmonyOS service/runtime behavior. The preceding `bcde79a` candidate runs the
unchanged production Node-API
module inside a controlled N-API runtime, invokes all 11 exports, and verifies
strict arity/type/handle/range rejection, status objects, bounded event arrays,
native handle finalization, recording ArrayBuffer round trips, and public bridge
recovery. The preceding `da4b708` candidate links the unchanged production
`AudioHost` to controlled OHAudio API 12 startup, fallback, PCM,
route/interruption/error, command, and persistence models. Both pass under
MSVC, Linux GCC, Emscripten, Clang ASan/UBSan, and AArch64 QEMU without claiming
HarmonyOS runtime evidence. The preceding `221abd9` candidate completes the
`68a4336` iOS hardware-key
simulation: `MOLViewController` directly consumes the tested C11 ownership
state for all 30 notes and Space sustain, with compile-time UIKit usage checks
in the Apple source. The preceding `b255bef` candidate creates and
inspects a real temporary Startup `.lnk`, launches the exact Windows daemon
through it with hidden-window policy, drives the exact CLI through the complete
null-audio lifecycle, retains the process handle through clean exit, and removes
the shortcut with the
production uninstaller without touching the user's Startup folder. The
preceding `938f955` candidate uses a real systemd 259 user manager to validate
and link a temporary unit retaining the shipped sandbox/restart policy, launches the real
daemon and CLI, and requires complete control/record/play/diagnostic coverage,
zero-exit shutdown, socket removal, and removal of the runtime unit link. The
preceding `a9b8e98` candidate runs the exact production Apple smoke
script unchanged against a controlled `launchctl`, `plutil`, and Darwin-host
model. That model launches the real daemon and CLI, enforces the expected plist
and executable contract, and verifies null-audio startup, state/capabilities,
preset and tempo control, note recording, playback, self-test, doctor, a finite
4,096-frame benchmark, all-notes-off, zero-exit shutdown, private socket cleanup,
and bootstrap/bootout. This is process-orchestration simulation and not native
Apple evidence. The Apple-only CTest and portable project audit remain
fail-closed. The preceding `f34aef3` candidate's CI-bound iOS
Simulator runner selects and boots an
available iPhone, installs and launches the real application bundle, and fails
unless the packaged production UI reaches the reply-capable native bridge for
both a valid `runtime.status` request and an invalid-version rejection. Its
project wiring is audited on every portable test configuration. The preceding
`a184c02` candidate's Objective-C++ iOS controller directly consumes a portable
production state machine for user-start gating, foreground/background
transitions, playback and metronome continuation, idle stop, route recovery,
and media-services reset. That same C source passes under MSVC, Linux GCC, and
Emscripten. The preceding `dbc4374` candidate's ESP32 and
ESP32-S3 images boot under Espressif QEMU, mount/format transactional storage,
pass the shared sequence and C4 checks, drain 12 production input commands, and
render more than 100,000 frames with non-silent finite output and zero project
failure counters. All four physical-board configurations still compile with
the emulator option disabled. Windows MSVC Release passes 91/91 tests, Linux
x86_64 GCC passes 92/92, and Emscripten MinSizeRel passes 42/42. System Chrome
on Windows and bundled Chromium on Linux each pass the five applicable desktop
application cases, including a real platform daemon process and authenticated
service controller. Current core coverage remains 94.10%, Clang static analysis passes
44 production translation units, and ASan/UBSan passes 57/57 with all twelve
fuzzers. The earlier `61b3342` candidate adds the
exact-production-source CoreAudio lifecycle simulation, and `18e6e7d` adds the
corresponding IOHID lifecycle simulation. The earlier `240b207` candidate's
dual-ABI Debug and instrumentation APKs, unsigned Release/R8/lintVital package,
and full lint gate
all pass. Android 15/API 35 emulator instrumentation also passes with injected focus loss/gain
and resumed AAudio callbacks. The prior `b3b7e14` candidate's
Linux AArch64 Release products pass the QEMU lifecycle/render gate, its AArch64
Debug suite passes 59/59, affected MSVC tests pass 5/5, and ESP32 HIL
parser/model self-tests pass 5/5. The prior
`4f77f56` candidate retains the complete MSVC Debug/LTO Release, Linux Clang,
ABI, sanitizer, analysis, endurance, size, package, and clean-checkout evidence
documented below. MSVC LTO Release passes 91/91 tests and its prior Debug run
passed 78/78; Emscripten MinSizeRel passes 42/42 and its prior Debug run passed
31/31. Windows and Linux shared-core builds pass 89/89 and 90/90
public-boundary tests, expose exactly the 47 version 1.0 API symbols, and Linux
ABI Compliance Checker reports 100% binary and source compatibility with zero
problems. GNU 15 Release+LTO Tiny, Standard, and Full profiles pass 91/91,
92/92, and 91/91. ASan/UBSan passes 47/47 including all
eleven fuzzers. LLVM-MinGW 20260826/Clang 23.1.0 and GNU 15.2.0 produced the
complete Windows ARM64 and Linux AArch64 products. QEMU evidence is not inferred
to be native ARM64 or physical-device evidence. Validation ran on 2026-09-03.

## Completed requirements

- The repository baseline, clean-room statement, Apache-2.0 licensing, privacy
  policy, bilingual introduction, CMake profiles, warnings-as-errors, dependency
  audit, locked miniaudio/Oboe sources, notices, and SPDX SBOM are present.
- `mol_core` is ISO C11 with a stable public C ABI and caller-owned aligned
  storage. Its render path has no allocation, blocking, logging, or language
  runtime callback.
- Static and shared builds install the same `mol::core` package. Shared builds
  hide every internal symbol and export exactly 47 `MOL_API` functions. The
  checked-in symbol and ABI Dumper baselines are enforced by a three-OS shared
  CI matrix and Linux ABI Compliance Checker.
- The native SDK gate installs into a fresh isolated prefix, resolves only that
  exported CMake package, compiles all public headers independently in C11 and
  C++17 modes, and runs standalone C and C++ consumers for static and shared
  libraries.
- The same core sources build and run under MSVC, Emscripten, and ESP-IDF. C and
  C++17 consumers link against the public API.
- The M1 path renders measured C4 through Native, WebAssembly/AudioWorklet,
  desktop miniaudio, and the ESP32 core/I2S host. Android Oboe, Apple AudioUnit,
  and Harmony OHAudio native entries use the shared fixed-memory callback
  runtime; Apple and Harmony SDK/device validation remains explicitly unclaimed.
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
- `mol-latency-probe` analyzes bounded multichannel PCM16 physical captures,
  requires route/device/buffer/commit metadata, and emits the capture SHA-256,
  sorted individual observations, P50/P95/maximum, unmatched counts, and an
  optional fail-closed P95 limit. Its deterministic 20-event fixture produces
  19.5/28.05/29 ms, while corrupt input and a 20 ms P95 limit are rejected.
- Patch, Mol Sequence, service configuration, JSON-RPC, MolWireEventV1, MIDI,
  latency/audio captures, ESP32 settings/Web forms, and HID-report Clang
  libFuzzer entries cover arbitrary bounded input and successful-parse round
  trips where applicable. ASan/UBSan builds all portable and control-plane
  tests; the Windows ASan runtime is deployed for all test binaries.
- Native and Wasm parse the exact same generated 12-event sequence fixture and
  match one golden summary. The ESP32 and ESP32-S3 startup paths parse those
  same checked-in bytes before I2S starts; both firmware targets compile. The
  current app binaries are 153,440 and 179,328 bytes respectively. Detailed M4
  evidence is in `docs/sequence/M4_SEQUENCE_EVIDENCE.md`.
- The desktop product now includes `mol-keyboardd`, `molctl`, miniaudio output
  and hotplug recovery, Windows Raw Input/Linux evdev/macOS IOHIDManager input
  adapters, bounded local IPC, all 41 specified JSON-RPC methods, strict atomic
  configuration, recording/playback, actionable doctor/self-test/benchmark,
  and user startup assets for Windows, systemd, and launchd.
- The desktop product includes `mol-keyboard`, a native wxWidgets frame that
  embeds the packaged production UI through WebView2, WebKit2GTK, or WKWebView.
  Its random `127.0.0.1` server rejects traversal and oversized requests,
  supplies the isolation/CSP headers required by AudioWorklet/Wasm, blocks
  popups and off-origin navigation, and stops with the window. Standalone GUI
  synthesis does not require or weaken the independently tested headless
  service.
- The optional `mol-keyboard-debug` is a fully native wxWidgets diagnostic
  client. Native controls cover service connection, preset/tempo/velocity,
  note and sustain input, recording, audio devices, self-test, doctor,
  benchmark, state/capability refresh, all-sound-off, and raw response logging.
  Its acceptance starts the real null-audio daemon and exercises IPC end to end.
- The shipped Linux systemd policy passes native user-manager acceptance under
  WSL: the runtime-only unit preserves restart and sandbox directives, starts
  the real daemon/CLI, exits successfully, removes its socket, and is unlinked.
- The Windows Startup installer creates the expected hidden WScript shortcut.
  An isolated-directory acceptance test launches the real daemon through that
  `.lnk`, completes the CLI lifecycle with exit code zero, then runs the
  production uninstaller and proves the shortcut and temporary state are gone.
- Checked-in cross toolchains build that complete desktop product, including
  `mol-keyboardd`, `molctl`, `mol-play`, `mol-render`, `mol-seq`,
  `mol-patchc`, `mol-audio-analyze`, and `mol_core`, as Windows ARM64 COFF and
  Linux AArch64 ELF. CI also includes native Windows and Ubuntu ARM64 runners.
- QEMU 10.2.1 executes the Linux AArch64 Release daemon, CLI, and renderer as a
  fail-closed product gate. A separate Debug target build passes 71/71 tests,
  including the 18-preset audio golden, local IPC, nested daemon/renderer
  processes, null playback, latency analyzer, and C/C++ consumers.
- The production desktop Web UI passed its applicable system-Chrome run on
  Windows and bundled-Chromium run on Linux. In both cases its service controller
  authenticated to the real platform daemon, delivered keyboard events,
  recorded a sequence, rejected a wrong token, and shut the daemon down cleanly.
- The exact macOS IOHID and CoreAudio-selected production sources compile and
  execute against controlled API models under MSVC and Linux Clang. Input
  enumeration/gesture cleanup and audio callbacks/device recovery pass, without
  claiming Apple SDK or native macOS evidence.
- The iOS controller directly consumes portable C11 background-policy and
  hardware-key ownership state. All 30 note usages plus Space sustain, repeat
  suppression, rejected-submit rollback, individual release, and bounded
  deactivation cleanup execute under MSVC, GCC, Emscripten, and Clang sanitizers.
- The Apple CTest graph includes a 30-second LaunchAgent product smoke which
  drives the real daemon and CLI over private local IPC with null audio and
  checks diagnostics, recording/playback, zero-exit shutdown, and socket
  cleanup. Linux executes the same runner against a controlled launchd process
  model and real product binaries; its portable integration audit also passes.
  Actual Apple launchd execution still requires an Apple runner.
- An independent Windows Release process used the active 48 kHz stereo WASAPI
  device, exposed the physical Raw Input adapter, passed doctor and a 96,000
  frame benchmark at 80.68 times realtime with no non-finite samples, and shut
  down with exit code 0. Linux/Clang ran the service lifecycle over a private
  Unix socket under WSL. Detailed evidence and unclaimed hardware boundaries
  are in `docs/service/M5_DESKTOP_EVIDENCE.md`.
- The complete bilingual Web/PWA product provides Explore and Studio controls,
  real AudioWorklet/Wasm synthesis, MessagePort and SharedArrayBuffer command
  paths, keyboard/multitouch gestures, IndexedDB recordings, offline service
  worker, and an authenticated loopback desktop-service controller. The browser
  matrix passed 15 applicable cases with 27 explicit capability skips across
  42 project/test combinations. Chrome, Edge, and Chromium mobile exercised
  realtime AudioWorklet output; Firefox exercised the same worklet/Wasm DSP in
  an `OfflineAudioContext` because its headless Windows process exposes no
  realtime audio output device. Detailed evidence and the unclaimed Safari
  boundary are in
  `docs/web/M6_WEB_EVIDENCE.md`.
- The Android application packages that same complete local UI without network
  permission, uses a bounded allow-listed JS bridge, and runs the shared core
  through JNI and Oboe in a legal `mediaPlayback` foreground service. Debug,
  unsigned Release, instrumentation, lint, both required ABIs, configurable
  application ID, privacy/notices packaging, route/focus handlers, foreground
  hardware keys, and private recording persistence are implemented. Mapped
  note keys are intercepted before WebView dispatch so UI focus cannot consume
  them; enqueue failures fall back instead of falsely claiming the event.
- The Android 15 x86_64 emulator exercised the real packaged UI-to-AAudio path
  at 48 kHz with zero render/non-finite failures, then exercised all 30 native
  key mappings and repeat suppression through production dispatch. Callback
  count advanced from 101 in background to 224 with the screen off, then the
  idle background stream and foreground state stopped. Injected transient
  focus loss stopped AAudio; focus gain reopened it and resumed finite
  callbacks. Detailed evidence and physical-device boundaries are in
  `docs/mobile/M7_ANDROID_EVIDENCE.md`.
- The iOS source now packages the same production UI with an offline-only
  WKURLSchemeHandler, Promise reply bridge, exact request schema, allow-listed
  commands, bounded event/recording transfer, foreground UIKit HID mapping,
  private atomic `.molseq` persistence, privacy manifest, localized metadata,
  and app icon. Its Objective-C++ controller restores persistent engine state
  after route/interruption/media-service rebuilds and keeps background audio
  only for playback or a running metronome transport. Xcode simulator/device
  pipelines are checked in. The controller-consumed policy state machine passes
  executable MSVC, Linux GCC, and Emscripten tests, but no Apple build, API
  execution, or device result is claimed on this host.
- The HarmonyOS Stage application provides the complete ArkUI surface and exact
  30-key foreground mapping, strict Node-API controls, OHAudio fast request with
  normal fallback and effective latency reporting, AudioSession focus,
  AVSession media commands, official audio-playback continuous tasks, private
  atomic recording persistence, and route/interruption restoration. The official
  OpenHarmony 5.0.0.71/API 12 public SDK now builds and audits Debug and Release
  compatibility HAPs with ArkTS bytecode plus AArch64 and x86-64 native audio
  libraries. The exact production background-policy `.ets` source executes
  without transformation under Node.js, is consumed directly by `AudioService`,
  and compiles into both HAPs. The exact production Node-API bridge and OHAudio
  host also pass
  controlled registration/validation, status/event/recording transfer, startup,
  fallback, PCM, route/interruption/error recovery, and cleanup tests across
  x64, Wasm, sanitizers, and AArch64 QEMU. This is build evidence for the
  compatibility product plus native-boundary simulation; no formal
  DevEco/HarmonyOS build or runtime result is claimed here.
- The M9 firmware now provides a configurable 5x6 GPIO matrix; shared BLE HID
  on both targets; Classic HID and A2DP Source/AVRCP on ESP32; USB boot HID on
  ESP32-S3; NVS settings and transactional FAT sequences; persisted HID/A2DP
  peers; physical configuration, clear-pairing, and factory-reset gestures; an
  isolated bounded control task; and an optional physically authorized WPA2
  SoftAP Web configuration service with strict Origin/token/form validation.
- ESP-IDF 6.1 built the default ESP32/ESP32-S3 images at 1,018,256 and 796,832
  bytes and the 4 MiB Web variants at 1,551,168 and 1,302,192 bytes. The core
  archive remains below 28 KiB and the queried eight-voice engine uses 37,664
  bytes of its 37,888-byte arena. A host-tested HIL verifier now fails on reset,
  underrun, watchdog, queue, persistence, capability, input, or real I2S-capture
  violations. Its virtual-clock mode passes 180 healthy snapshots for each
  chip family and rejects reset, deadline, stalled-audio, and firmware-error
  injection without claiming board execution.
- Isolated QEMU configurations for both chips execute the real ESP-IDF 6.1
  bootloader/application, NVS/FAT startup, the shared sequence and C4 checks,
  the production input/control path, and the statically allocated FreeRTOS
  audio task through a paced virtual PCM sink. The runner rejects missing
  phases, project errors, non-finite or silent audio, physical-peripheral
  startup, and failure counters, then writes `emulated-firmware` evidence with
  explicit physical and real-time exclusions.

## In-progress work

- The desktop-first local audit is complete: Windows/Linux application and
  service process paths pass, while macOS platform-specific source paths have
  executable simulations and a CI-bound LaunchAgent product smoke. Native
  macOS application/service acceptance is the highest-priority external gate.
  The iOS production background-policy and hardware-key ownership state
  machines, the HarmonyOS production native bridge/host simulations, and
  strongest reachable device-free ESP32 firmware execution gate also pass;
  mobile and ESP32 external acceptance follows the desktop gate.
  Documentation remains a draft and `v1.0.0` remains forbidden until all
  results pass.

## Blocked platform checks

- Apple SDKs and DevEco/HarmonyOS SDKs are not available on this Windows host.
  The public OpenHarmony SDK compatibility build and macOS simulations do not
  change those formal platform constraints or runtime status.
- Playwright's Windows WebKit port does not expose AudioWorklet and is not actual
  Safari. Current-stable Safari remains unverified until run on an Apple host.
- No Bluetooth output was exposed for the Windows run. WSL exposes neither a
  physical evdev keyboard nor native Linux audio hardware. Those M5 acceptance
  paths and macOS compilation/runtime remain unverified.
- Windows ARM64 and Linux AArch64 binaries are build-verified, but no native
  ARM64 host was available locally. Linux AArch64 QEMU execution covers target
  instructions, service IPC, rendering, and tests, but cannot verify native
  scheduling, input, audio, latency, or hardware lifecycle. Native ARM64 CI jobs
  are configured but an unpushed local commit is not reported as a CI result.
- Physical Android/Apple/Harmony/ESP32 devices, I2S capture equipment, signing
  credentials, and long-run device time are not available. The Android emulator
  result, ESP-IDF builds, and Espressif QEMU firmware runs are not promoted to
  device verification.

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

The two desktop GUIs and the portable package were validated separately:

```powershell
cmake -S . -B build/desktop-gui -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  -DMOL_BUILD_DESKTOP_GUI=ON -DMOL_BUILD_NATIVE_DEBUG_GUI=ON `
  -DMOL_BUILD_TESTS=ON
cmake --build build/desktop-gui
ctest --test-dir build/desktop-gui --output-on-failure
cmake --preset package-release
cmake --build --preset package-release
cpack --config build/package-release/CPackConfig.cmake -B build/packages
python tools/package_audit.py `
  --archive build/packages/mol-keyboard-0.1.0-Windows-AMD64.zip `
  --report-dir build/package-audit --expected-version 0.1.0
```

The GUI-enabled Windows MSVC suite passes 98/98. The current ordinary Debug
suite passes 99/99 after adding the Android emulator runner and project audits.
The real system WebView2
reports `PASS-SharedArrayBuffer`; the native debugger connects to the real
daemon and completes state/capability/self-test/note/shutdown RPC. Linux GCC
passes 100/100 with both GUI tests under Xvfb; WebKitGTK reports
`PASS-MessagePort`. Package audit launches the extracted production GUI and
native service debugger before running the independent extracted headless
runtime lifecycle.

MSVC 19.51.36248 passes 95/95 tests in the current LTO Release build; the prior
Debug build passed 78/78. These runs include the iOS production lifecycle
policy, exact HarmonyOS production policy source, strict Web form protocol, and
HIL evidence-parser tests in addition to
the independent daemon process, realtime runtime, local IPC, all service
methods, CLI validation, configuration restart, recording/playback, native
WinMM MIDI and bounded decoder paths, the macOS IOHID/CoreAudio/CoreMIDI
lifecycle simulations, and prior core/tool coverage. Dedicated
GNU 15 Release+LTO presets pass 91/91 for Tiny, 92/92 for Standard, and 91/91
for Full with the required Node.js and Python runtimes. The Full run exercises
64 voices, 4,096 sequence events, the complete desktop daemon, and the expanded
fixed host arenas.

Under WSL, Linux x86_64 GCC 15.2.0 builds the current tree and passes 95/95
tests; the prior Clang 21.1.8 candidate passed 78/78. The current suite runs a
real systemd user unit lifecycle plus the production macOS service smoke
unchanged through a controlled launchd model, the production raw-MIDI adapter
through a kernel FIFO, and the real Linux daemon/CLI. The daemon process used its null sink and a private
Unix socket; the production Web UI additionally controlled it under bundled
Chromium. Node.js is a fail-closed native-test dependency: set `EMSDK_NODE` when
it is not on `PATH`. Physical Linux devices and native Apple behavior remain
unclaimed.

Static/shared API parity is checked separately:

```sh
cmake --preset ci-shared
cmake --build --preset ci-shared
ctest --preset ci-shared --output-on-failure
nm --dynamic --defined-only --format=posix \
  build/ci-shared/libmol_core.so.0.1.0 \
  | cut -d ' ' -f 1 | LC_ALL=C sort > build/mol_core-current.symbols
diff -u abi/mol_core-1.0.symbols build/mol_core-current.symbols
abi-dumper build/ci-shared/libmol_core.so.0.1.0 \
  -lver current -o build/mol_core-current.dump
abi-compliance-checker -l mol_core -old abi/mol_core-1.0.dump \
  -new build/mol_core-current.dump \
  -report-path build/mol_core-abi-report.html
```

Windows and Linux shared builds pass 89/89 and 90/90 tests, respectively. The
current dynamic library matches all 47 expected symbols exactly; ABI Compliance
Checker reports 100% binary and source compatibility, zero problems, and zero
warnings. Independent installed C11 and C++17 consumers also compile and execute
against the Windows and Linux shared packages.

With the pinned Emscripten environment active:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug --output-on-failure
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release --output-on-failure
```

Emscripten 6.0.5 and Node.js 22.16.0 pass 46/46 tests in the current LTO
MinSizeRel build; the prior Debug candidate passed 31/31. Both configurations
match the Native event, sequence-fixture, and 18-preset audio-metric goldens.

The production Web bundle passed 12/12 Node tests. Playwright 1.62.1 ran 42
browser project/test combinations: 15 applicable cases passed and 27 were
explicitly skipped by capability. System Chrome 151.0.7922.175, system Edge
152.0.4191.53, Firefox 153.0, Chromium 151.0.7922.34 mobile emulation, and
WebKit 26.5 desktop/mobile rendering were covered. Chrome and Edge exercised
the realtime AudioWorklet; Firefox executed the real worklet and Wasm DSP in
an offline audio graph because the headless runner exposes no realtime output
device. Chrome also reloaded offline, started audio, played a note, and observed
the core event. Actual Safari is not claimed.

The desktop application path was refreshed at repository candidate `4cdb929`.
Independent lockfile installs on Windows and in a clean Linux checkout reported
zero vulnerabilities; both passed 12/12 Node tests, strict type checking, and
the production Vite build. Windows system Chrome 151.0.7922.175 and Linux
bundled Chrome for Testing 151.0.7922.34 each passed five applicable desktop
cases with two capability-specific skips. Each service-controller run spawned
the real platform daemon and verified authenticated WebSocket control, engine
events, service recording, invalid-token rejection, IPC shutdown, and a clean
process exit.

With JDK 21, Android API 36, Build Tools 36.0.0, NDK 28.2.13676358, and CMake
3.31.6 installed:

```powershell
& "$env:EMSDK/emsdk_env.ps1"
platforms/android/build-app.ps1 Debug
$env:MOL_SKIP_WEB_BUILD = "1"
platforms/android/build-app.ps1 DebugAndroidTest
platforms/android/build-app.ps1 Release
Push-Location platforms/android
./gradlew.bat :app:lintDebug --no-daemon --no-configuration-cache
Pop-Location
```

The exact clean `0dbce9e` pipeline compiled all 108 Wasm actions, passed 46/46
Wasm tests and 12/12 Web tests, completed TypeScript strict checking and the
production UI build, and built the dual-ABI Android application. Debug,
unsigned Release, device-test APKs, R8/lintVital, and full Debug lint all
passed. Archive inspection confirmed the paired `mol_audio_worklet_core.js`
and `.wasm` assets and both `lib/arm64-v8a/libmol_android_audio.so` and
`lib/x86_64/libmol_android_audio.so`. The official Android 15/API 35 x86_64
headless emulator returned AAudio API 2, 48 kHz, 28,800 rendered frames at the
foreground checkpoint, all 30 hardware keys, repeat suppression, a stopped
runtime after injected transient focus loss, 4 callbacks after focus-gain
reopen, 101 background callbacks, 224 screen-off callbacks, no
render/non-finite failure, successful idle shutdown and service cleanup, and
instrumentation code `-1`.

For sanitizer and parser fuzz validation, activate the Visual Studio environment
and place Clang 22 on `PATH`:

```powershell
cmake --preset fuzz-clang
cmake --build --preset fuzz-clang
ctest --preset fuzz-clang --output-on-failure
```

The current ASan/UBSan configuration passed 57/57 tests. Patch, Mol Sequence,
service configuration, JSON-RPC, MolWireEventV1, MIDI-file import, realtime MIDI
streaming, latency/audio captures, ESP32 settings/Web forms, and HID-report
libFuzzer smoke sessions each ran for 20 seconds and produced no finding.
Accepted MIDI files are reparsed and compared through the canonical sequence
JSON representation; realtime streams must emit identical commands when fed as
one buffer or at arbitrary chunk boundaries and must release every gesture.

The latency analyzer's five native tests pass in static and shared suites. The
synthetic fixture checks its 20 raw observations and 19.5/28.05/29 ms
P50/P95/maximum, but is explicitly not physical product evidence. Real route
commands and fail-closed acquisition rules are in
`docs/testing/LATENCY_MEASUREMENT.md`; all hardware rows remain open.

With the pinned ESP-IDF environment active, from `platforms/esp32`:

```powershell
./build-target.ps1 -Target esp32
./build-target.ps1 -Target esp32s3
./build-target.ps1 -Target esp32 -WebUi
./build-target.ps1 -Target esp32s3 -WebUi
```

All four ESP-IDF 6.1/GNU 15.2.0 variants rebuilt successfully. Default/Web
image sizes are 1,018,256/1,551,168 bytes for ESP32 and
796,832/1,302,192 bytes for ESP32-S3. Default ESP32 reports 101,892 of 124,580
bytes DRAM; its Web variant reports 118,000 bytes. Default ESP32-S3 reports
148,923 of 341,760 bytes DIRAM; its Web variant reports 187,975 bytes. These
are build/map results, not physical playback results.

The optional Espressif QEMU 9.2.2 gate executed the clean `dbc4374` images:

```powershell
platforms/esp32/run-qemu.ps1 -Target esp32
platforms/esp32/run-qemu.ps1 -Target esp32s3
```

ESP32 passed after 105,728 frames, 12 drained commands, and 211,256 nonzero
samples; ESP32-S3 passed after 101,888 frames, 12 commands, and 203,852 nonzero
samples. Both had three snapshots, 262.5 Hz C4, zero non-finite samples, and
zero project failure counters. Each emulator run counted two deadline misses;
the 22,459/26,857 microsecond maximum render times are reported but excluded from
real-time acceptance. Full hashes and physical exclusions are in
`docs/hardware/M9_ESP32_EVIDENCE.md`.

Additional locally actionable M10 gates passed with these reproducible command
families:

```sh
cmake --preset coverage-clang
cmake --build --preset coverage-clang
python3 tools/coverage_gate.py --build-dir build/coverage-clang --source-dir .
cmake --preset static-analysis-clang
cmake --build --preset static-analysis-clang
python3 tools/static_analysis.py --build-dir build/static-analysis-clang
cmake --preset tsan-clang
cmake --build --preset tsan-clang
ctest --preset tsan-clang
cmake --preset endurance
cmake --build --preset endurance
ctest --test-dir build/endurance --output-on-failure -L endurance
```

The complete desktop products also pass the checked-in cross-build presets:

```sh
# Ubuntu/Debian system cross compiler, or an unpacked sysroot selected through
# MOL_LINUX_AARCH64_ROOT.
cmake --preset ci-linux-aarch64
cmake --build --preset ci-linux-aarch64
```

With QEMU user mode and the AArch64 runtime prefix available, the target test
binaries and final products execute under emulation:

```sh
cmake --preset ci-linux-aarch64-qemu
cmake --build --preset ci-linux-aarch64-qemu
ctest --preset ci-linux-aarch64-qemu
python3 tools/aarch64_emulation_gate.py \
  --qemu /usr/bin/qemu-aarch64 --sysroot /usr/aarch64-linux-gnu \
  --build-dir build/ci-linux-aarch64 \
  --artifact-commit "$(git rev-parse HEAD)" \
  --report build/aarch64-emulation-report.json
```

The local QEMU 10.2.1 run passed 71/71 AArch64 tests in 93.02 seconds. The
Release product report for `b3b7e14` passed daemon/CLI IPC, record/playback,
doctor, self-test, finite benchmark, clean shutdown, and deterministic WAV
validation. It is explicitly `simulated-runtime`, not native/device evidence.

```powershell
# Extract LLVM-MinGW 20260826 UCRT, then select it explicitly.
$env:MOL_LLVM_MINGW_ROOT = "C:\path\to\llvm-mingw-20260826-ucrt-x86_64"
cmake --preset ci-windows-arm64-cross
cmake --build --preset ci-windows-arm64-cross
```

The Linux result contains AArch64 ELF executables; the Windows result contains
COFF-ARM64 executables and archive members. Both were inspected with target
object tools after the build. The LLVM-MinGW archive used locally was
190,721,391 bytes with SHA-256
`ae601f4e0f72bbdf441ad2df8bb16f037e2e9251559ea6b37b4057aef39c06c3`.

Coverage passed at 94.10% overall, including 95.95% queue/memory, 97.78%
music-state, 97.49% Patch, and 95.57% Sequence coverage. Clang static analysis
passed all 44 first-party production translation units. Linux Clang
ThreadSanitizer passed 40/40 tests in 15.62 seconds. The refreshed GCC 15
optimized endurance suite passed 2/2 in 270.38 seconds: the engine simulated
1,800 seconds in 268.567 seconds (6.70x realtime, approximately 14.92% of one
core), emitted 230,136 events, and produced no non-finite samples. Runtime
recovery completed 30 rebuild cycles in 1.42 seconds.

The refreshed release size gate passed at 505,956 bytes for the stripped core,
976,136 bytes for daemon plus CLI, 23,018 bytes for gzip-compressed Wasm, and
157,610 bytes for deployable Web resources. Dependency locks, notices,
licenses, npm audit, and SPDX SBOM validation passed. Current exact-candidate
CPack package audits found 153 files on Windows and 152 on Linux, including the
native wxWidgets production executable, optional native debugger, WebView2
loader where required, `mol-latency-probe`, and every desktop service
definition. Each extracted production GUI loads the packaged Web bundle and
passes real system-WebView capability checks. Each native debugger then drives
and shuts down its own packaged daemon before a separate daemon and CLI complete
the no-UI null-audio lifecycle with native MIDI capability. The Windows AMD64
ZIP is 4,463,145 bytes with SHA-256
`5c4cfff1a1272f42828d7b911f72f54d1dee009c3da78589145800e4558efd34`;
its WebView2 reports SharedArrayBuffer and its recording is 327 bytes. The Linux
x86_64 TGZ is 7,732,484 bytes with SHA-256
`10521374f9ebc222d507eb99138191190b371d879bc7138f849d6b9050b6f0ae`;
its WebKitGTK reports MessagePort and its recording is 326 bytes. Both report
schema 4, 48 kHz stereo, 4,096 finite benchmark frames, no non-finite samples,
exit code zero, and successful IPC cleanup. They are unsigned 0.1.0 candidate
archives, not releases.

```sh
python3 tools/release_size_gate.py \
  --native-core build/package-release/libmol_core.a \
  --daemon build/package-release/apps/mol-keyboardd/mol-keyboardd \
  --cli build/package-release/apps/molctl/molctl \
  --wasm build/wasm-release/platforms/web/emscripten/mol_core.wasm \
  --web-dir apps/web/dist --report-dir build/size-report
cpack --config build/package-release/CPackConfig.cmake -B build/packages
python3 tools/package_audit.py --archive <archive> \
  --report-dir <report-directory> --expected-version 0.1.0
```

A clean local clone of exact candidate
`68978434f077cc9170e29f17fe5829ab7d11da97`, created without hardlinks or copied
build output, rebuilt the production Web/Wasm assets, passed 44/44 Emscripten
MinSizeRel tests, compiled all 575 Windows package actions, and passed all three
extracted runtime paths with the Windows hash recorded above. A fresh Linux
Release+LTO build of the identical committed source compiled 543 actions and
passed the same three-path package audit with the Linux hash recorded above.
The clean clone remained clean after verification.

The preceding clean clone of
`3e63c2e7f3a6436baf49be5c123765ee85a9b404`, created without hardlinks or copied
build output, compiled 680 Windows MSVC build actions and passed 98/98
Release+LTO tests with both GUI applications. The same clone compiled 652 Linux
GCC build actions and passed 100/100 Release+LTO tests with both GUIs under
Xvfb, then compiled all 108 Emscripten targets and passed 43/43 MinSizeRel tests.
The two native runs each installed the SDK into a fresh nested prefix, built
all 28 independent public
header translation units plus C11 and C++17 consumers, and passed both installed
consumer tests. The Windows suite created a real temporary hidden Startup
shortcut and drove the real headless daemon through the CLI before clean
shutdown and uninstall. The Linux suite ran the real daemon/CLI through the
actual systemd 259 user manager. Its macOS service case used the same production
daemon and CLI against a controlled launchd model, so it remains simulation and
not native Apple evidence. Both clean native builds produced fresh packages;
their extracted production GUI and complete null-audio service lifecycle pass
with the hashes recorded above.

An earlier clean local clone of `75609b6f32c25f430cb618e9e56a04e9391406f6` with no
copied repository build output compiled all 167 Linux GCC targets. Its first
configure exposed that a missing Node executable silently reduced the suite to
89 tests. The official Node.js 22.16.0 Linux x64 archive was then verified
against its published SHA-256
`F4CB75BB036F0D0EDDF6B79D9596DF1AAAB9DDCCD6A20BF489BE5ABE9467E84E`;
with `EMSDK_NODE` pointing to that native binary, the checkout passed 90/90.
This included the exact HarmonyOS production-policy source, the real systemd
259 user-service lifecycle, and the controlled launchd model driving the real
Linux daemon/CLI. Candidate `00a0e50` closes the discovered omission by making
the no-Node configuration fail explicitly; both that negative configure probe
and the complete 90/90 Linux suite pass on the resulting tree.

A clean local clone of `098ddf4360b03318f1efabd741bd0bcf6a76dbf7` with no
copied repository build output or managed-dependency directory rebuilt 167
MSVC targets and passed 89/89 LTO Release tests. A separate clean Emscripten
build compiled 108 targets and passed 41/41 MinSizeRel tests. The same checkout
ran the OpenHarmony Release wrapper from an empty output tree; its explicit
clean, strict ArkTS checking, native compilation, packaging, and 13-entry audit
passed. That run produced an unsigned 2,953,954-byte HAP with run-specific
archive SHA-256
`3AE2368D3A2F1901390B6C56164A26DE8A0AC07700DD512F84FBEE6664FAE312` and
packaged `ets/modules.abc` SHA-256
`C5DB99A0968F9514BC37BEEB6731E1E4BCF12BB3E14C640BB647C945077C545B`.
The raw HAP ZIP digest is not used as a cross-run reproducibility identity
because Hvigor records archive timestamps. Two independent clean clones of the
unchanged application source produced byte-identical content for all 13
extracted HAP entries, including the same bytecode digest.

An earlier clean local clone of `67a9e5138692991839121ae57c8df38abfa6d701` with no
copied repository build output or managed-dependency directory rebuilt 154
MSVC targets and passed 82/82 LTO Release tests. A separate clean Emscripten
build compiled 95 targets and passed 35/35 MinSizeRel tests. Both new Apple
acceptance scripts retained Git mode `100755` and were executable after the
clone. The clone reused only the externally installed pinned Emscripten SDK;
its configure explicitly selected the Ninja executable shipped with Visual
Studio because Ninja is not on this host's default `PATH`.

The earlier clean clone of `d0fa3d6f96fe0df9e5dd61ae944bc5a9e1e6e030`
also ran a clean `npm ci` with zero vulnerabilities, the 12/12 Web unit suite,
strict TypeScript checking, the production bundle, and fresh ESP32/ESP32-S3
QEMU builds through the external official ESP-IDF 6.1 installation. Both
firmware images passed their three-snapshot runtime gates with 107,904 and
104,960 rendered frames respectively, 12 input commands each, finite
non-silent output, and zero render or write failures.

## Known failures

No locally reproducible implementation, build, test, sanitizer, analysis,
package, or documentation failure is currently known. The unavailable external
acceptance runs listed above remain open gates rather than skipped or passing
tests.

## Known environment constraints

- `cl`, Emscripten, ESP-IDF, and the Clang ASan runtime are activated through
  their toolchain environments and are not all on the default `PATH`.
- Test-enabled CMake configuration requires Python 3 and Node.js; unavailable
  runtimes stop configuration instead of reducing the registered test set.
- Cross-platform source checks are not promoted to device verification.

The HarmonyOS application descriptors, project audit, executable production
policy/bridge/host simulations, and native source-check boundary pass locally.
The previously verified headless Windows MSVC and Linux GCC Release suites pass
95/95; the GUI-enabled Release+LTO suites pass 98/98 on Windows and 100/100 on
Linux. The latest exact-candidate Emscripten suite passes 46/46, and Linux
AArch64 QEMU passes 71/71.
The official OpenHarmony 5.0.0.71/API 12 public SDK and Hvigor
5.8.9 build and audit
both Debug and Release compatibility HAPs; the Release artifact is an unsigned
2,953,954-byte package. The clean candidate run recorded archive SHA-256
`3AE2368D3A2F1901390B6C56164A26DE8A0AC07700DD512F84FBEE6664FAE312` and
stable packaged bytecode SHA-256
`C5DB99A0968F9514BC37BEEB6731E1E4BCF12BB3E14C640BB647C945077C545B`;
the raw archive hash includes timestamp metadata. The audited package contains
ArkTS bytecode and both required native ABIs.
`platforms/harmony/build-app.sh release` remains the fail-closed formal DevEco
lane. Detailed evidence and pending formal/device acceptance are in
`docs/mobile/M8_HARMONY_EVIDENCE.md`.

## Next highest-priority task

Run the checked-in `native` and `macos-desktop` jobs on a real macOS 15 host
first. The latter uses `cmake --preset ci-macos-desktop`, real WKWebView and
native-debugger acceptance, and the extracted GUI/headless package audit. Then
exercise current Safari plus CoreAudio, IOHID permission, launchd, route loss,
and clean shutdown. After the desktop gate, run native Windows/Linux ARM64
execution, official iOS and Harmony builds, physical Android/iOS/Harmony lifecycle and
route checks, ESP32/ESP32-S3 30-minute HIL with I2S/A2DP/USB/GPIO evidence, and
instrumented P50/P95/maximum latency on every required route. Keep `v1.0.0`
blocked until all results pass and the exact final candidate is reviewed.
