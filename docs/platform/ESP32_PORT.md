# ESP32 Device Port

The reference firmware builds the exact portable `mol_core` sources as an
ESP-IDF component. Both targets stream stereo PCM16 through ESP-IDF's
standard-mode I2S TX driver, host BLE keyboards, scan a 5x6 GPIO matrix, load
settings from NVS, and store sequences transactionally on a FAT wear-levelled
partition. The original ESP32 also hosts Classic Bluetooth keyboards and an
A2DP Source; ESP32-S3 instead hosts USB boot keyboards and never advertises
Classic A2DP. The default firmware does not require PSRAM.

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
voice. The default physical recovery operations are one-shot gestures:

- hold key 29 for three seconds to enter configuration mode;
- hold keys 29 and 28 for five seconds to clear stored HID/A2DP peers and
  remove Bluetooth bonds;
- hold keys 29 and 27 for ten seconds to erase settings, sequences, Web
  credentials, and pairing state, then restart with safe defaults.

Every operation first releases the GPIO source. Every pin, timing, velocity,
ghost policy, task setting, recovery key, and queue bound is configurable in
Kconfig. The pure state machine has host tests for debounce, ghost suppression,
one-shot holds, cancellation, and chord precedence.

## HID and A2DP hosts

The bounded boot-keyboard translator accepts only keyboard interfaces and
maps HID usages into the same gesture/command queue used by GPIO. BLE and
Classic discovery, reconnect, report delivery, and bond removal run outside
the audio task. A newly connected peer address is persisted by the control
task. ESP32-S3 additionally uses Espressif's pinned USB host/HID components;
its internal PHY uses GPIO 19/20 and requires an external current-limited 5 V
VBUS source.

On the original ESP32, A2DP uses the ESP-IDF Source and AVRCP Controller APIs.
The audio task only performs a bounded copy into a fixed PCM ring and never
waits for SBC encoding or the Bluetooth stack. Discovery, authentication,
connect/reconnect, media control, delay reporting, and peer persistence are
handled by lower-priority control callbacks. I2S remains active as the safe
fallback. ESP32-S3 compiles the A2DP capability out.

## Optional local Web configuration

`./build-target.ps1 -Target <target> -WebUi` creates an explicit 4 MiB firmware
variant. The default build contains no HTTP server. A Web build still exposes
nothing until the physical configuration hold is completed and persisted
settings permit the UI. It then starts a WPA2 SoftAP and binds HTTP only to that
interface for a ten-minute default window. The random 16-hex AP password and
32-hex form token live in NVS; only the password is printed after physical
authorization, and the token is never logged.

The page has no external resources. POST requests require the exact AP Origin,
the exact form content type, a constant-time token match, a body no larger than
512 bytes, known unique fields, strict percent encoding, and finite bounded
numeric values. Settings are queued to the isolated control task, so neither
HTTP nor NVS work enters the audio task. Factory reset erases the credentials.

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
minimum internal-heap watermark, non-finite render samples, and nonzero PCM
samples every ten seconds. The startup log also records the reset reason.

These counters make underruns and resets diagnosable, but a successful compile
does not prove hardware output. Device verification requires flashing the
matching target, observing the C4 conformance message, hearing stable I2S audio,
and recording a sustained-play run with zero failure and deadline counters.

## Espressif QEMU smoke test

After installing ESP-IDF's optional `qemu-xtensa` tool, run the actual firmware
without a board from the repository root:

```powershell
& .\.cache\esp-idf\export.ps1
platforms/esp32/run-qemu.ps1 -Target esp32
platforms/esp32/run-qemu.ps1 -Target esp32s3
```

The isolated `build-<target>-qemu` configuration replaces physical I2S pacing
with a virtual PCM sink and prevents GPIO, Bluetooth, A2DP, and USB startup.
Everything before the sink remains production code: ESP-IDF boot, NVS and FAT,
the shared core, startup conformance, the bounded input queue, device control,
and the statically allocated FreeRTOS audio task. The runner stores an ignored
serial log and `emulated-firmware` JSON report under `build/qemu-<target>/` and
fails on missing startup phases, silence, non-finite output, project errors,
unexpected physical-peripheral startup, or failure counters.

This is stronger than the separate virtual-clock HIL-parser model, but it is
not board evidence. QEMU timing, GPIO, radio, USB, I2S, audio, power, and
endurance results are explicitly excluded. Never flash or release an image
with `CONFIG_MOL_QEMU_RUNTIME=y`.

## Verified build procedure

After activating the pinned ESP-IDF environment:

```powershell
cd platforms/esp32
./build-target.ps1 -Target esp32
./build-target.ps1 -Target esp32s3
./build-target.ps1 -Target esp32 -WebUi
./build-target.ps1 -Target esp32s3 -WebUi
```

Target configuration is stored in each build directory, so both target builds
coexist without sharing an `sdkconfig`. On 2026-09-03, ESP-IDF 6.1 and GNU
15.2.0 produced these map-backed results:

| Target | Application image | `libmol_core.a` | Data-memory map |
|---|---:|---:|---:|
| ESP32 | 1,018,256 bytes | 26,790 bytes | DRAM 101,892 / 124,580 bytes |
| ESP32-S3 | 796,832 bytes | 26,519 bytes | DIRAM 148,923 / 341,760 bytes |
| ESP32 + Web | 1,551,168 bytes | 27,015 bytes | DRAM 118,000 / 124,580 bytes |
| ESP32-S3 + Web | 1,302,192 bytes | 27,087 bytes | DIRAM 187,975 / 341,760 bytes |

The component builds the complete M3 Tiny graph and stores all 18 fixed
120-byte compiled Patches in flash. Its complete code and read-only archive is
under 28 KiB, far below the 512 KiB gate. The host reserves 37,888 bytes for an
eight-voice engine; the target query currently requires 37,664 bytes. This is
below the 256 KiB working-memory gate and requires no PSRAM. The Web profile's
original-ESP32 link margin is intentionally called out for runtime heap HIL;
build success is not treated as device proof.

The exact flash, serial-monitor, input, I2S-capture, A2DP, and 30-minute
acceptance procedure is in `docs/hardware/M9_ESP32_EVIDENCE.md`. No physical
board result is claimed until that procedure produces a passing report.
