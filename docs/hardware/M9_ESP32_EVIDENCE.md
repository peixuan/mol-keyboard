# M9 ESP32 Hardware Evidence

## Current result

M9 implementation is complete and both default and optional-Web variants are
build-verified with ESP-IDF 6.1/GNU 15.2.0. No ESP32 or ESP32-S3 board, I2S
capture instrument, Bluetooth speaker, or hardware keyboard is attached to
this host. Therefore I2S electrical/audio output, radio interoperability,
physical input, watchdog-free 30-minute operation, and Web service startup are
not device-verified. This file defines the required reproducible test and does
not substitute build output for hardware evidence.

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
and negative tests for resets, deadline failures, and capability mismatches:

```powershell
python tests/hardware/esp32_hil.py --self-test
ctest --preset dev-debug -R mol_esp32_hil_parser_tests --output-on-failure
```

These tests validate the evidence parser only; they do not emulate a board.
