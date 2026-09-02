# Known Limitations

The repository has completed M0 and is implementing M1. The native core and
PCM16 WAV renderer produce a single polyBLEP saw instrument with ADSR and fixed
polyphony, and the same core runs in a verified browser AudioWorklet. There is
not yet a realtime desktop host, service, complete PWA, or end-user application.
The ESP-IDF firmware now contains a compile-verified I2S host and startup C4
analysis, but has not run on a physical board; I2S signal, sound, timing
counters, and sustained operation are therefore not device verified. PCM24 and
float WAV output are not implemented yet.

The current Windows host has Visual Studio 2026, MSVC 19.51.36248, CMake 4.4.0,
and Ninja 1.13.2 in its developer environment. Emscripten 6.0.5 and ESP-IDF 6.1
are provisioned in ignored local caches. Hardware, background-audio, latency,
underrun, and device-routing claims therefore remain unverified.
