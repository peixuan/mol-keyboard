# ESP32 Port

The ESP-IDF project in `platforms/esp32` compiles the exact `mol_core` source
used by native and WebAssembly builds. The `mol_core` component adds only Tiny
profile compile definitions; it does not fork or patch the core.

## Verified build procedure

After activating the pinned ESP-IDF environment:

```powershell
cd platforms/esp32
idf.py -B build-esp32 set-target esp32
idf.py -B build-esp32 build
idf.py -B build-esp32s3 set-target esp32s3
idf.py -B build-esp32s3 build
```

Target configuration is stored in each build directory, so ESP32 and ESP32-S3
builds coexist without sharing `sdkconfig`. On 2026-09-02, ESP-IDF 6.1 with GNU
15.2.0 produced these size results:

| Target | Application image | `libmol_core.a` contribution |
|---|---:|---:|
| ESP32 | 108,992 bytes | 2,664 bytes flash code |
| ESP32-S3 | 121,648 bytes | 2,680 bytes flash code |

The current firmware performs a bounded, static-memory C4 synthesis self-test at
startup. Build success is not device verification: the startup result must be
captured from a physical board before runtime or device status is claimed.

I2S, GPIO/HID, persistence, configuration mode, and original-ESP32 A2DP Source
are subsequent M1/M9 work. ESP32-S3 will never advertise Classic Bluetooth A2DP
Source capability.
