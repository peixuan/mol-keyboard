# Known Limitations

The repository is transitioning from M0 to M1. The native core and PCM16 WAV
renderer produce a single polyBLEP saw instrument with ADSR and fixed polyphony,
but there is not yet a realtime audio host, WebAssembly module, firmware,
firmware, service, or application. The verified WebAssembly module is currently
a worker/Node-compatible core bridge, not an AudioWorklet or complete Web app.
PCM24 and float WAV output are not implemented yet. Platform entries beyond the
Windows x64 and WebAssembly core remain plans, not support claims.

The current Windows host has Visual Studio 2026, MSVC 19.51.36248, CMake 4.4.0,
and Ninja 1.13.2 in its developer environment. Emscripten 6.0.5 is provisioned
in an ignored local cache; ESP-IDF is not installed. Hardware, background-audio,
latency, underrun, and device-routing claims therefore remain unverified.
