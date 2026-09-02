# Known Limitations

The repository has completed M0 and is implementing M1. The native core and
PCM16 WAV renderer produce a single polyBLEP saw instrument with ADSR and fixed
polyphony, but there is not yet a realtime desktop host, AudioWorklet, I2S audio
task, service, or end-user application. The verified WebAssembly module is
currently a worker/Node-compatible core bridge, not a complete Web app. The
ESP-IDF firmware is compile-verified and includes a startup render self-test,
but has not run on a physical board. PCM24 and float WAV output are not
implemented yet.

The current Windows host has Visual Studio 2026, MSVC 19.51.36248, CMake 4.4.0,
and Ninja 1.13.2 in its developer environment. Emscripten 6.0.5 and ESP-IDF 6.1
are provisioned in ignored local caches. Hardware, background-audio, latency,
underrun, and device-routing claims therefore remain unverified.
