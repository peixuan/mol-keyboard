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

Android, Apple, and HarmonyOS currently provide native audio call entries, not
complete M7/M8 product applications. Their platform packaging, legal background
audio, complete UI shells, route/focus handling, and physical-device tests are
the next implementation and device gates.
