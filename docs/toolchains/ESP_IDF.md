# ESP-IDF Toolchain

The ESP32 build is pinned to ESP-IDF 6.1 at Git revision
`fff9895c82d744c7237be8847347bdd1b07c6643`. The SDK is an Apache-2.0 build
dependency and is not vendored or distributed with MoL Keyboard.

## Provisioning

Clone the exact release and install only the two supported Xtensa targets:

```powershell
git clone --depth 1 --branch v6.1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git .cache/esp-idf
.cache/esp-idf/install.bat esp32,esp32s3
.cache/esp-idf/export.bat
idf.py --version
```

The verified Windows environment reported:

- ESP-IDF 6.1;
- Python 3.14.6 in the ESP-IDF-managed virtual environment;
- CMake 4.0.3 and Ninja 1.12.1 from ESP-IDF;
- `xtensa-esp-elf` GNU 15.2.0 (`esp-15.2.0_20251204`);
- optional Espressif QEMU 9.2.2 (`esp_develop_9.2.2_20260417`).

## Reproducible target builds

```powershell
cd platforms/esp32
idf.py -B build-esp32 set-target esp32
idf.py -B build-esp32 build
idf.py -B build-esp32s3 set-target esp32s3
idf.py -B build-esp32s3 build
```

The project stores `sdkconfig` in its selected build directory. This avoids
cross-target configuration conflicts and permits subsequent independent builds.
Successful compilation is build evidence only. The optional isolated QEMU
profile supplies real firmware boot/runtime evidence without physical-device
claims:

```powershell
python .cache/esp-idf/tools/idf_tools.py install qemu-xtensa
& .\.cache\esp-idf\export.ps1
platforms/esp32/run-qemu.ps1 -Target esp32
platforms/esp32/run-qemu.ps1 -Target esp32s3
```

The QEMU profile executes ESP-IDF, storage, the production input/control path,
the shared core, and the FreeRTOS audio task with a virtual PCM sink. Flashing,
physical GPIO/radio/USB/I2S/audio, real-time timing, power, and endurance still
require the corresponding physical device and instruments.

## I2S host and size evidence

The firmware uses the ESP-IDF standard-mode I2S API with six fixed 128-frame DMA
descriptors. Target-specific reference pins and every board-facing I2S setting
are exposed through Kconfig. See `docs/platform/ESP32_PORT.md` for wiring,
configuration, task priority/core affinity, and diagnostic counters.

Generate map-backed component evidence after either build:

```powershell
idf.py -B build-esp32 size-components
idf.py -B build-esp32s3 size-components
```

On 2026-09-02 the complete M3 Tiny build produced 146,704-byte ESP32 and
172,560-byte ESP32-S3 app binaries. `libmol_core.a` contributes 20,422 and
20,646 bytes respectively, including 2,532 bytes of compiled Patch flash data.
The firmware now reserves a 131,072-byte static engine arena for the complete
voice/effect graph. The size tool reports 147,164 / 180,736 bytes of internal
DRAM used on ESP32 and 172,655 / 341,760 bytes of internal memory used on
ESP32-S3. These measurements include all 18 preset IDs.
