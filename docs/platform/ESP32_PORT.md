# ESP32 I2S Port

The reference firmware builds the exact portable `mol_core` sources as an
ESP-IDF component and streams stereo PCM16 through ESP-IDF's standard-mode I2S
TX driver. It uses six fixed 128-frame DMA descriptors by default and does not
require PSRAM.

## Reference wiring

The defaults target a generic ESP32-WROOM-32 development board and an external
3.3 V I2S DAC such as a MAX98357A:

| Signal | ESP32-WROOM-32 | ESP32-S3-DevKitC-1 | DAC pin |
|---|---:|---:|---|
| Bit clock | GPIO 26 | GPIO 4 | BCLK |
| Word select | GPIO 25 | GPIO 5 | LRC / WS |
| Serial data | GPIO 22 | GPIO 6 | DIN |
| Ground | GND | GND | GND |

Power the selected DAC according to its board documentation. The reference
host does not drive MCLK. Codecs that require MCLK can assign it through
`CONFIG_MOL_I2S_MCLK_GPIO`.

GPIO assignments are firmware configuration, never engine state. Run
`idf.py menuconfig`, open **MoL Keyboard audio host**, and change BCLK, WS,
DOUT, optional MCLK, sample rate, DMA descriptor count, PCM16 dither, task
priority, task stack size, or core affinity for another board. Confirm that the
chosen pins are broken out and are not reserved for flash, PSRAM, or boot straps
on that exact module.

## Runtime model and diagnostics

At startup the firmware renders and analyzes one second of C4 before enabling
the output device. A dedicated statically allocated task, priority 20 pinned to
core 1 by default, then renders 128 frames at a time. The task performs
saturated float-to-PCM16 conversion and feeds the preallocated I2S DMA queue;
optional triangular dither is deterministic.

The I2S driver automatically clears a late TX buffer to silence. The render
task never waits on storage or networking, and a 100 ms I2S write timeout keeps
it recoverable. It is subscribed to the task watchdog. A lower-priority control
task reports rendered frames, render/write failures, partial writes, DMA event
queue overflows, render deadline misses, maximum render time, watchdog errors,
minimum audio-stack headroom, and minimum internal-heap watermark every ten
seconds. The startup log also records the reset reason.

These counters make underruns and resets diagnosable, but a successful compile
does not prove hardware output. Device verification requires flashing the
matching target, observing the C4 conformance message, hearing stable I2S audio,
and recording a sustained-play run with zero failure and deadline counters.

## Verified build procedure

After activating the pinned ESP-IDF environment:

```powershell
cd platforms/esp32
./build-target.ps1 -Target esp32
./build-target.ps1 -Target esp32s3
```

Target configuration is stored in each build directory, so both target builds
coexist without sharing an `sdkconfig`. On 2026-09-02, ESP-IDF 6.1 and GNU
15.2.0 produced these map-backed results:

| Target | Application image | `libmol_core.a` flash code | Host static DRAM |
|---|---:|---:|---:|
| ESP32 | 124,256 bytes | 2,660 bytes | 22,168 bytes |
| ESP32-S3 | 149,520 bytes | 2,672 bytes | 22,168 bytes |

GPIO/HID input, persistence, configuration mode, and original-ESP32 A2DP Source
remain M9 work. ESP32-S3 never advertises Classic Bluetooth A2DP Source.
