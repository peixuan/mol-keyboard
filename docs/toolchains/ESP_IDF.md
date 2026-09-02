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
- `xtensa-esp-elf` GNU 15.2.0 (`esp-15.2.0_20251204`).

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
Successful compilation is build evidence only; flashing, startup logs, I2S
output, and timing measurements require the corresponding physical device.
