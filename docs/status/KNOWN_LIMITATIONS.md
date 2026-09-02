# Known Limitations

The M0-M4 gates, M5 desktop implementation, and M6 Web/PWA implementation are
complete. The current Windows host has Visual Studio 2026, MSVC 19.51.36248,
CMake 4.4.0, Ninja 1.13.2, Emscripten 6.0.5, and ESP-IDF 6.1. Linux/Clang service
validation runs under WSL with null audio. macOS compilation and physical
Bluetooth audio/keyboard verification remain unavailable here.

The production PWA is runtime-verified with current system Chrome and Edge and
the pinned Firefox engine. Playwright WebKit verifies shared rendering and
mobile layout, but its Windows port does not expose AudioWorklet and is not
actual Safari. Current-stable Safari and physical mobile audio/lifecycle claims
therefore remain an Apple-device gate.

The ESP-IDF firmware contains a build-verified I2S host and shared-sequence
startup check, but has not run on a physical board. I2S signal, sound, timing,
Bluetooth/GPIO input, sustained operation, and ESP32 Classic A2DP Source remain
unverified.

Android now provides a complete dual-ABI application and is runtime-verified on
an Android 15 x86_64 emulator, including AAudio rendering, legal foreground
state, background/screen-off continuation, and idle shutdown. No physical
Android device was available, so arm64 playback, hardware keyboard, actual
Bluetooth or wired route changes, focus interruption, latency, and sustained
playback remain unverified.

The iOS application implementation is complete, including the packaged shared
UI, Promise-based WKWebView bridge, AudioUnit lifecycle restoration, hardware
key interception, private sequence persistence, offline content policy, privacy
manifest, and Xcode simulator/device pipeline. This Windows host has no Xcode,
iOS Simulator, signing identity, or physical Apple device, so compilation,
installation, audible output, background/lock-screen continuation,
route/interruption behavior, hardware keyboard input, latency, and sustained
playback are not claimed.

The HarmonyOS Stage application implementation is also complete, including the
full native ArkUI surface, strict Node-API/OHAudio runtime, fast-to-normal
latency fallback and reporting, AudioSession focus, AVSession controls,
audio-playback continuous-task policy, private sequence persistence, and HAP
build audit. This host has no DevEco Studio, HarmonyOS SDK/toolchain, signing
identity, or physical device. HAP construction, installation, sound, background
and screen-off playback, route/focus/interruption recovery, latency, and
sustained playback are therefore not claimed.
