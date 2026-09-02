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

### Reference 5x6 GPIO keyboard

The enabled reference matrix maps row-major key indexes 0-29 directly to
MIDI C4-F6. Rows are outputs normally held high; the scan task pulls one row low
at a time. Columns are active-low inputs with internal pull-ups. Place a switch
at each row/column intersection:

| Matrix line | ESP32-WROOM-32 | ESP32-S3-DevKitC-1 |
|---|---:|---:|
| Row 0-4 | GPIO 13, 14, 16, 17, 18 | GPIO 7, 8, 9, 10, 11 |
| Column 0-5 | GPIO 19, 21, 23, 27, 32, 33 | GPIO 12, 13, 14, 15, 16, 17 |

For a diode matrix, orient every diode consistently and select
`MOL_GPIO_GHOST_ALLOW`. The default no-diode policy freezes all keys in an
ambiguous two-row/two-column rectangle, so a phantom press can never create a
note and existing notes do not receive false releases. The pure matrix state
machine is covered by host tests for 5-20 ms debounce, deterministic transition
order, both ghost policies, overflow transactions, and the one-shot long hold.

The scan task has static storage, priority 12 on core 0, and a 1 ms period by
default. It assigns a unique gesture to every press and sends note commands
through a static bounded queue. Only the audio task calls the engine. A dropped
transition schedules a source-wide all-notes-off recovery to prevent a stuck
voice. Holding key index 29 for three seconds requests configuration mode and
also releases the GPIO source. Every pin, timing, velocity, ghost policy, task
setting, configuration key, and queue bound is configurable in Kconfig.

## Runtime model and diagnostics

At startup the firmware renders and analyzes one second of C4, then reinitializes
the engine to silence before enabling the output device. A dedicated statically
allocated task, priority 20 pinned to core 1 by default, drains the bounded input
queue and renders 128 frames at a time. The task performs
saturated float-to-PCM16 conversion and feeds the preallocated I2S DMA queue;
optional triangular dither is deterministic.

The I2S driver automatically clears a late TX buffer to silence. The render
task never waits on storage or networking, and a 100 ms I2S write timeout keeps
it recoverable. It is subscribed to the task watchdog. A lower-priority control
task reports rendered frames, render/write failures, partial writes, DMA event
queue overflows, render deadline misses, maximum render time, watchdog errors,
submitted/rejected/dropped input commands, queue high-water, GPIO scans,
transitions, ambiguous scans, delivery recovery, minimum task-stack headroom,
and minimum internal-heap watermark every ten seconds. The startup log also
records the reset reason.

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
coexist without sharing an `sdkconfig`. On 2026-09-03, ESP-IDF 6.1 and GNU
15.2.0 produced these map-backed results:

| Target | Application image | `libmol_core.a` | Internal memory used / available |
|---|---:|---:|---:|
| ESP32 | 161,312 bytes | 26,133 bytes | 156,940 / 180,736 bytes |
| ESP32-S3 | 187,136 bytes | 26,383 bytes | 182,519 / 341,760 bytes |

The component builds the complete M3 Tiny graph and stores all 18 fixed
120-byte compiled Patches in flash. The host reserves a 131,072-byte static
engine arena and now uses all 12 Tiny voices; a matching native budget test
guards that exact configuration. No PSRAM is required by this baseline.

HID input, persistence, the configuration service, and original-ESP32 A2DP
Source remain M9 work. ESP32-S3 never advertises Classic Bluetooth A2DP Source.
