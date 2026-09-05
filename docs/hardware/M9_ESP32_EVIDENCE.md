# M9 ESP32 Hardware Evidence

## Current result

M9 implementation is complete, both default and optional-Web variants are
build-verified with ESP-IDF 6.1/GNU 15.2.0, and isolated ESP32 and ESP32-S3
firmware images execute successfully in Espressif QEMU. No ESP32 or ESP32-S3
board, I2S capture instrument, Bluetooth speaker, or hardware keyboard is
attached to this host. Therefore I2S electrical/audio output, radio
interoperability, physical input, watchdog-free 30-minute operation, and Web
service startup are not device-verified. This file defines the required
reproducible tests and does not substitute emulation or build output for
hardware evidence.

## Device-free firmware execution

Install the optional QEMU tool from the pinned ESP-IDF checkout, activate the
environment, and run both targets from the repository root:

```powershell
python .cache/esp-idf/tools/idf_tools.py install qemu-xtensa
& .\.cache\esp-idf\export.ps1
platforms/esp32/run-qemu.ps1 -Target esp32
platforms/esp32/run-qemu.ps1 -Target esp32s3
```

The wrapper builds with `sdkconfig.defaults.qemu`, runs the real ESP-IDF image
on its matching Espressif QEMU machine, and fails unless it observes one clean
boot, NVS/FAT startup, the shared 12-event sequence, finite C4 conformance, the
production FreeRTOS audio/control tasks, a command drained through the
production input queue, three diagnostic snapshots, non-silent PCM, and zero
render, write, queue, storage, watchdog, or non-finite-sample errors. QEMU uses
a paced virtual PCM sink because its emulated I2S, radio, USB, and GPIO are not
product peripherals. The dedicated Kconfig option makes those physical hosts
unselectable in an emulator image and is disabled in every board build.

The 2026-09-05 exact-source run used QEMU 9.2.2
`esp_develop_9.2.2_20260417`, ESP-IDF 6.1, and clean commit `19735a9`.
The same checkout first rebuilt every board configuration with the emulator
option disabled:

| Target | Result | App image | App-partition margin |
|---|---|---:|---:|
| ESP32 | passed | 1,018,320 bytes | 30,256 bytes (3%) |
| ESP32-S3 | passed | 796,896 bytes | 251,680 bytes (24%) |
| ESP32 + Web | passed | 1,551,232 bytes | 152,704 bytes (9%) |
| ESP32-S3 + Web | passed | 1,302,256 bytes | 401,680 bytes (24%) |

The isolated QEMU configurations then produced these results:

| Target | Result | Final frames / commands / nonzero samples | App image SHA-256 |
|---|---|---:|---|
| ESP32 | passed | 96,000 / 12 / 191,853 | `97f95d1871bebe8fad791deb6b997a7b606adba95e5387b1ee76c223968f10e7` |
| ESP32-S3 | passed | 98,176 / 12 / 196,195 | `071b4f7eb50ea30b158182603b9c592a1cbfad381ae2f084d8750792c6ff0845` |

Both reports recorded three snapshots, C4 at 262.5 Hz with 0.058417 peak,
zero non-finite samples, and zero project failure counters. The ESP32 and
ESP32-S3 emulator runs respectively counted 156 and 139 render-deadline misses
and maximum render times of 27,121 and 27,306 microseconds. Emulator wall-clock
performance is deliberately excluded from acceptance, so those values are
reported but are neither ignored hardware failures nor real-time evidence.
The ignored `build/qemu-<target>/report.json` and `serial.log` files retain the
complete local result and artifact hashes.

This gate is `emulated-firmware` evidence. It does not claim physical silicon,
GPIO/HID/USB/Bluetooth/RF/I2S/acoustic behavior, real-time scheduling,
watchdogs, power, thermals, or endurance, and therefore does not close the
release HIL gate below.

## Bench topology

Use two independent benches:

1. An ESP32-WROOM-32 development board, separate UART bridge if needed, the
   documented I2S DAC or logic/audio capture adapter, a BLE or Classic boot
   keyboard, and a discoverable A2DP speaker.
2. An ESP32-S3-DevKitC-1, UART console, the documented I2S capture adapter, a
   BLE keyboard, and a USB boot keyboard. Supply USB VBUS from an external
   current-limited 5 V source; never source peripheral current from a GPIO.

Wire the 5x6 matrix and I2S signals exactly as documented in
`docs/platform/ESP32_PORT.md`. The I2S capture must be derived from the physical
BCLK/WS/DOUT signals and exported as stereo PCM16 WAV at the firmware sample
rate. A synthesized or offline-rendered WAV is not evidence.

## Automated long run

Activate the pinned ESP-IDF environment from the repository root:

```powershell
& .\.cache\esp-idf\export.ps1
```

For the original ESP32, use the Web variant so the same run covers physical
configuration authorization and A2DP selection. During the run, hold the
configuration key, join the private AP using the printed password, select A2DP,
pair the speaker, play every matrix key, and play the Bluetooth keyboard:

```powershell
platforms/esp32/run-hil.ps1 `
  -Target esp32 -Port COM5 -DurationMinutes 30 -WebUi `
  -I2sCaptureWav .\build\captures\esp32-i2s.wav `
  -RequireGpio -RequireBluetooth -RequireA2dp -RequireClearPairing
```

For ESP32-S3, play all matrix keys, then the BLE and USB keyboards. Enter the
Web configuration mode once so AP startup and timeout are exercised:

```powershell
platforms/esp32/run-hil.ps1 `
  -Target esp32s3 -Port COM6 -DurationMinutes 30 -WebUi `
  -I2sCaptureWav .\build\captures\esp32s3-i2s.wav `
  -RequireGpio -RequireBluetooth -RequireUsb -RequireClearPairing
```

The wrapper rebuilds the exact target, flashes it, hard-resets through esptool,
and records serial text plus a JSON report under the ignored
`build/hil-<target>/` directory. The verifier fails unless it observes:

- exactly one boot, the shared 12-event sequence check, bounded engine memory,
  finite C4 frequency/peak, I2S, GPIO, control, and target-specific capability
  startup;
- enough ten-second diagnostic snapshots and at least 90% of the expected
  audio-frame progress after startup allowance;
- zero render, I2S write, partial-write, DMA overflow, deadline, watchdog,
  input, persistence, HID, USB, and requested A2DP failure counters;
- the requested physical GPIO/Bluetooth/USB/A2DP/configuration/pair-clearing
  counters;
- a real five-second-or-longer stereo PCM16 I2S capture at the firmware rate,
  with non-silent RMS/peak and no clipped samples.

`-SerialOnly` deliberately waives physical I2S capture and is useful only for
bench diagnosis. A serial-only pass must not be promoted to device-verified.

## Manual checks not folded into the long run

Run these after saving the long-run report because factory reset intentionally
causes another boot:

1. Hold the clear-pairing chord. Confirm the log reports bond-removal requests,
   the old HID keyboard and A2DP speaker do not reconnect automatically, and
   explicit rediscovery can pair them again.
2. Save a non-default preset, tempo, output, and a sequence; power-cycle without
   reflashing; confirm NVS and FAT recovery restore them.
3. Hold the factory-reset chord for the configured ten seconds. Confirm the
   completion log, software restart, safe defaults, missing sequence, forgotten
   peers, and newly generated Web credentials.
4. Leave configuration mode idle. Confirm HTTP and SoftAP stop after the
   configured window while I2S frame diagnostics continue.
5. On ESP32-S3, confirm the capability log says Classic A2DP is absent. On the
   original ESP32, record the sink-reported A2DP delay separately; do not apply
   the wired latency threshold.

Record board revision, module/flash size, DAC/capture model, keyboard and
speaker models, power source, wiring revision, firmware commit, complete HIL
JSON reports, I2S analyzer output, and operator/date in the release evidence.
Do not commit raw device logs, credentials, or local absolute paths.

## Host validation of the verifier

The HIL parser is part of normal CTest. It has passing ESP32/ESP32-S3 fixtures
and negative tests for resets, deadline failures, and capability mismatches.
Test-enabled CMake configuration now requires Python, so these cases cannot
silently disappear when the interpreter is unavailable:

```powershell
python tests/hardware/esp32_hil.py --self-test
ctest --preset dev-debug -R mol_esp32_hil_parser_tests --output-on-failure
```

The deterministic virtual-clock mode advances a complete 30-minute telemetry
session without waiting 30 wall-clock minutes. It requires modeled GPIO and
Bluetooth activity, Web authorization and pair clearing, original-ESP32 A2DP
PCM progress, ESP32-S3 USB HID activity, 57,600,000 I2S frames, and 180 healthy
diagnostic snapshots. Negative self-tests inject a reset, deadline miss,
stalled audio, and firmware error and require every case to fail closed:

```powershell
python tests/hardware/esp32_hil.py --simulate --target esp32 `
  --report build/esp32-simulated-hil.json
python tests/hardware/esp32_hil.py --simulate --target esp32s3 `
  --report build/esp32s3-simulated-hil.json
python tests/hardware/esp32_hil.py --simulate --target esp32 `
  --inject-fault deadline-miss
```

The final command is expected to fail. Every report is labeled
`simulated-hil` and lists excluded physical claims. These tests validate the
evidence parser and long-run acceptance logic only; they do not execute
firmware, emulate a board, or replace UART/I2S/radio evidence.
