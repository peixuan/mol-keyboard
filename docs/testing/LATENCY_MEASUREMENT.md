# End-to-End Latency Measurement

## Evidence boundary

This procedure measures physical stimulus-to-output latency. Engine timestamps,
audio API buffer estimates, synthetic WAV files, emulator results, and offline
render speed do not close a physical latency gate. Run every required route on
the exact candidate commit and retain the report plus the captured WAV outside
the repository's source history.

`mol-latency-probe` accepts bounded, uncompressed multichannel PCM16 RIFF/WAVE
captures. It detects rising edges on one trigger channel, finds the first
response threshold crossing in a configured time window on another channel,
and reports the sorted individual measurements, P50, linearly interpolated P95,
and maximum. It records the capture SHA-256 and required route metadata.

## Acquisition setup

Use one synchronized acquisition interface with at least two input channels:

1. Feed channel 1 from the physical trigger sensor or electrical switch signal.
   The signal must mark the real input actuation presented to the product, not a
   timestamp generated later in software.
2. Feed channel 2 from the product response. Prefer an electrical loopback from
   a wired output or I2S decoder. For built-in speakers and Bluetooth sinks, use
   a fixed microphone and report its model, placement, and acoustic environment.
3. Record at 48 kHz or higher without automatic gain control, noise suppression,
   echo cancellation, or independent channel clocks. Preserve the original
   interleaved PCM16 WAV.
4. Produce at least 20 isolated events. Space triggers beyond both the configured
   refractory interval and maximum response window. Exercise the same input path
   used by the acceptance scenario.
5. Record the host/board, input device, output device or sink, OS/browser and
   versions, physical connections, effective sample rate, channel layout, and
   actual audio buffer configuration.

Inspect the waveform before analysis. Choose thresholds above the measured
noise floor and below every valid pulse/onset. A response already above its
threshold at the search start invalidates onset interpretation and requires a
cleaner recording or a documented threshold adjustment.

## Analysis

Build the optimized native tools, then analyze a capture:

```sh
cmake --preset dev-release
cmake --build --preset dev-release
build/dev-release/tools/latency-probe/mol-latency-probe capture.wav \
  --report latency-wired.json \
  --route built-in-wired \
  --device "input interface; output device; connection" \
  --buffer-config "48000 Hz; 128 frames; 3 periods" \
  --artifact-commit "$(git rev-parse HEAD)" \
  --trigger-channel 1 --response-channel 2 \
  --trigger-threshold 0.5 --response-threshold 0.1 \
  --min-latency-ms 1 --max-latency-ms 200 \
  --refractory-ms 300 --minimum-events 20 \
  --p95-limit-ms 50
```

Use the equivalent `.exe` path on Windows and pass the commit explicitly from
PowerShell. Tune the search window only to the documented route. Increase it for
Bluetooth rather than silently discarding slow responses.

The process fails when the capture is malformed, metadata is missing, channels
or options are invalid, fewer than the required events match, or a supplied P95
limit is exceeded. Any unmatched trigger requires waveform review even when the
minimum match count was reached.

## Route acceptance

Create a separate capture and report for each row:

| Route | Acceptance |
| --- | --- |
| Representative built-in or wired output | P95 target 30 ms; required maximum 50 ms |
| USB audio | Record P50/P95/max and the effective device buffer; apply 50 ms only when used as the representative wired route |
| Desktop Web Audio | P95 at most 50 ms on each claimed supported browser/host |
| System Bluetooth audio | Record P50/P95/max; omit `--p95-limit-ms` |
| ESP32 I2S | Record P50/P95/max with board, codec/DAC, I2S format, and DMA configuration |
| ESP32 A2DP | Record P50/P95/max on original ESP32; omit `--p95-limit-ms` |

Do not combine route distributions. If multiple browsers or materially different
buffer configurations are release claims, report each separately. A failed limit,
missing trigger, clipped onset, changed route, underrun, device reset, or capture
clock discontinuity invalidates the run and must not be averaged away.

## Analyzer self-test

The deterministic fixture validates only the parser and statistics:

```sh
mol-latency-probe --generate-fixture synthetic-latency.wav
mol-latency-probe synthetic-latency.wav \
  --report synthetic-latency.json --route synthetic \
  --device fixture --buffer-config deterministic \
  --artifact-commit self-test --p95-limit-ms 50
```

It contains 20 artificial observations from 10 to 29 ms. Its expected P50 is
19.5 ms, P95 is 28.05 ms, and maximum is 29 ms. Never copy these values into a
platform evidence row.
