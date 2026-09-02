# Known Limitations

The repository has completed the M0, M1, and M2 architecture and music-semantics
gates and is implementing M3. The native core and PCM16 WAV renderer still
produce a single polyBLEP saw instrument with ADSR and fixed polyphony; the 18
data-driven instruments and Chorus, Delay, Reverb, and Limiter are not complete.
The same core runs in a verified browser AudioWorklet and Windows WASAPI
callback. There is not yet a service, complete PWA, or end-user application.
The ESP-IDF firmware now contains a compile-verified I2S host and startup C4
analysis, but has not run on a physical board; I2S signal, sound, timing
counters, and sustained operation are therefore not device verified. PCM24 and
float WAV output are not implemented yet.

The desktop host has not been built or run on Linux or macOS. The current
Windows host has Visual Studio 2026, MSVC 19.51.36248, CMake 4.4.0,
and Ninja 1.13.2 in its developer environment. Emscripten 6.0.5 and ESP-IDF 6.1
are provisioned in ignored local caches. Hardware, background-audio, latency,
long-duration underrun, end-to-end latency, and device-routing recovery claims
therefore remain unverified.

Android, Apple, and HarmonyOS currently provide M1 native call entries, not
complete applications. The Android and Harmony sources pass strict host-side
source compilation; the Apple entry is source-reviewed only. Platform SDK
builds, packaging/signing, legal background-audio integration, actual
low-latency status, and physical-device tests remain M7/M8 work.
