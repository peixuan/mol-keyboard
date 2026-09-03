# Known Limitations

The M0-M4 gates, M5 desktop implementation, and M6 Web/PWA implementation are
complete. The current Windows host has Visual Studio 2026, MSVC 19.51.36248,
CMake 4.4.0, Ninja 1.13.2, Emscripten 6.0.5, and ESP-IDF 6.1. Linux/Clang service
validation runs under WSL with null audio. The production desktop Web UI now
controls real Windows and Linux service processes in browser automation. The
shipped Linux service policy also passes real systemd 259 user-manager startup,
sandboxed execution, clean shutdown, socket cleanup, and runtime-unit removal.
The Windows installer/uninstaller and an actual WScript shortcut pass an
isolated Startup-directory product lifecycle; this does not alter the real user
Startup folder or substitute for a sign-out/sign-in launch observation.
macOS compilation and physical Bluetooth audio/keyboard verification remain
unavailable here.

The exact macOS IOHID adapter and CoreAudio-selected desktop runtime compile and
execute against controlled API models under MSVC and Linux Clang. These tests
cover input gesture ownership and cleanup plus audio callback, device selection,
notification, recovery, and cleanup state. A fail-closed Apple-only CTest now
bootstraps a temporary user LaunchAgent from the shipped template and drives the
real daemon/CLI lifecycle with null audio, including zero-exit and socket-cleanup
checks. Linux also executes the unchanged runner against a controlled launchd
model and the real product binaries, validating the service contract,
bootstrap/bootout, full control flow, clean exit, and cleanup. Its cross-platform
project audit passes, but the Apple CTest itself has not run here. These checks
do not provide Apple framework headers or ABI, native launchd behavior, system
permissions, hardware routes, or native macOS
scheduling, so macOS remains below `build-verified` and `runtime-verified`.

The complete Windows ARM64 and Linux AArch64 products cross-build and their
object formats were inspected as COFF-ARM64 and AArch64 ELF respectively. They
have not executed on a native ARM64 host. QEMU 10.2.1 now passes 70/70 Linux
AArch64 target tests and an end-to-end null-audio daemon/CLI/render gate, which
checks ISA execution and process behavior but not native scheduling, audio,
input, latency, or hardware lifecycle. Native ARM64
GitHub runner jobs are checked in, but no result is claimed before the commits
are present on a remote workflow run.

The production PWA is runtime-verified with current system Chrome and Edge.
The pinned Firefox engine executes the real AudioWorklet and Wasm DSP through
offline rendering, but its headless Windows process exposes no realtime output
device; Firefox realtime audio output is therefore not claimed from this run.
Playwright WebKit verifies shared rendering and mobile layout, but its Windows
port does not expose AudioWorklet and is not actual Safari. Current-stable
Safari and physical mobile audio/lifecycle claims remain an Apple-device gate.

All locally actionable M10 automated gates, including clean-checkout
reproduction and audited portable packages, pass. The remaining release gates
all require unavailable official platform toolchains, physical devices/routes,
or loopback measurement equipment. This is why the repository truthfully
remains at 0.1.0 and has no `v1.0.0` tag.

The `mol-latency-probe` executable, deterministic analyzer tests, malformed
capture fuzzer, and fail-closed physical acquisition procedure are complete.
No synchronized two-channel capture equipment or required output routes were
available, so the synthetic 19.5/28.05/29 ms fixture statistics are tool tests
only and every platform latency row remains unmeasured.

The M9 ESP-IDF implementation is complete and all four ESP32/ESP32-S3 default
and optional-Web variants build. Isolated real ESP-IDF images for both chips
also boot in Espressif QEMU and pass storage, shared-sequence/C4 conformance,
production input/control, FreeRTOS audio, finite/non-silent output, and bounded
runtime diagnostics through a virtual sink. It has not run on a physical board.
I2S signal, sound, real-time timing, GPIO/BLE/Classic/USB input, NVS/FAT
power-cycle recovery, physical AP authorization, sustained operation, and
ESP32 Classic A2DP Source remain unverified. The ESP32 Web map leaves 6,580
bytes in the primary DRAM layout before runtime allocations, so its
Wi-Fi/Bluetooth heap watermark is a mandatory HIL result, not an inferred
capability. The fail-closed procedure and exact QEMU boundary are documented in
`docs/hardware/M9_ESP32_EVIDENCE.md`. A separate virtual-clock model validates
180 ten-second telemetry snapshots for each chip family and proves reset,
deadline, stalled-audio, and firmware-error injection are rejected; it is
parser/control evidence, while QEMU is firmware evidence, and neither is
hardware evidence.

Android now provides a complete dual-ABI application and is runtime-verified on
an Android 15 x86_64 emulator, including AAudio rendering, legal foreground
state, an injected transient focus-loss/gain cycle, background/screen-off
continuation, and idle shutdown. The injection calls the same service listener
used by Android and verifies actual AAudio stop/reopen, but is not evidence from
a competing application. No physical
Android device was available, so arm64 playback, hardware keyboard, actual
Bluetooth or wired route changes, external focus arbitration, latency, and sustained
playback remain unverified.

The iOS application implementation is complete, including the packaged shared
UI, Promise-based WKWebView bridge, AudioUnit lifecycle restoration, hardware
key interception, private sequence persistence, offline content policy, privacy
manifest, and Xcode simulator/device pipeline. The exact background-policy
state machine consumed by the Objective-C++ controller passes executable tests
under MSVC, Linux GCC, and Emscripten. The controller also directly consumes an
executable C11 hardware-key ownership state machine: all 30 note usages, Space
sustain, repeat suppression, failed-submit rollback, gesture IDs, and
deactivation cleanup pass across those toolchains plus targeted Clang
ASan/UBSan. This does not simulate UIKit event delivery. CI also contains a
fail-closed `simctl` runner which installs and launches the real Simulator app,
verifies the packaged production UI, calls the reply-capable bridge, checks
invalid-version rejection, and captures logs plus a screenshot. This validates application
state transitions and acceptance wiring, not Apple APIs or scheduling. This
Windows host has no Xcode, iOS Simulator, signing identity, or physical Apple
device, so compilation, installation, Simulator execution, audible output,
background/lock-screen continuation,
route/interruption behavior, hardware keyboard input, latency, and sustained
playback are not claimed.

The HarmonyOS Stage application implementation is also complete, including the
full native ArkUI surface, strict Node-API/OHAudio runtime, fast-to-normal
latency fallback and reporting, AudioSession focus, AVSession controls,
audio-playback continuous-task policy, private sequence persistence, and HAP
build audit. The official public OpenHarmony 5.0.0.71/API 12 SDK produces
audited Debug and Release compatibility HAPs with both required native ABIs.
The unchanged production Node-API bridge and OHAudio host pass controlled API
execution for registration, validation, status/events/recording transfer,
fast/normal startup, PCM rendering, route/interruption/error recovery, and
cleanup under x64, Wasm, sanitizers, and AArch64 QEMU. This simulates the native
boundary, not ArkTS execution, AudioSession, AVSession, continuous-task delivery,
HarmonyOS scheduling, or audio hardware.
This host still has no DevEco Studio, formal HarmonyOS SDK/toolchain, signing
identity, emulator, or physical device. A formal signed HarmonyOS HAP,
installation, sound, background and screen-off playback,
route/focus/interruption recovery, latency, and sustained playback are therefore
not claimed.
