# Known Limitations

The repository is at M0. The verified core lifecycle currently renders silence;
it does not yet ship an audio runtime, service, application, firmware, or package.
Platform entries beyond the Windows x64 M0 core remain plans, not support claims.

The current Windows host has Visual Studio 2026, MSVC 19.51.36248, CMake 4.4.0,
and Ninja 1.13.2 in its developer environment. Emscripten and ESP-IDF are not
installed. Hardware, background-audio, latency, underrun, and device-routing
claims therefore remain unverified.
