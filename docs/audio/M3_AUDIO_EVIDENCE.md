# M3 Audio and DSP Evidence

This document records the evidence used to close the M3 quality gate. It does
not claim listening tests or physical-device playback that did not occur.

## Signal path

Each voice owns a snapshot of its selected compiled Patch and combines the
selected procedural model, oscillator, optional noise, state-variable filter,
ADSR, velocity curve, vibrato, saturation, and instrument gain. The six model
families are subtractive, two-operator FM, additive, Karplus-Strong pluck,
modal, and formant synthesis.

Voice output is routed by per-Patch chorus, delay, and reverb sends. The stereo
mix then passes through a smoothed master gain, DC blockers, limiters with a
-1 dBFS default ceiling, and a bounded 128-frame output transition. Chorus,
delay, and reverb state is fixed-capacity caller-owned memory; their parameters
are smoothed and effect tails can be cleared without allocation.

## Loudness calibration and golden sequence

The checked-in `tests/golden/m3_audio_metrics.txt` sequence is fixed at 48 kHz,
stereo, MIDI C4, velocity 0.8, a one-second gate, and a four-second render. Patch
gains were calibrated against integrated RMS from that sequence. The 18 final
RMS measurements span 0.011469 to 0.012139, while peak, attack, release,
stereo-difference, maximum-step, spectral centroid, and low/mid/high spectral
energy remain preset-specific.

The golden verifier permits absolute cross-architecture differences of 0.0015
peak, 0.0003 RMS, 20 ppm DC, 0.0003 stereo difference, 0.0015 maximum step,
3 ms attack, 20 ms active-end timing, 10 Hz spectral centroid, and 10 permille
for each spectral band. Golden tests only read the fixture. Updating it requires
an explicit `mol_audio_metrics` run, review of the audio/analyzer report, a
manual fixture edit, and a reason in `tests/golden/README.md`.

`mol-audio-analyze` independently reads bounded PCM16 RIFF/WAVE files and
reports PCM hash, peak/RMS/DC, clipped samples, estimated fundamental,
attack/active-end/tail, Hann-window spectral energy, stereo difference, maximum
sample step, and click candidates. Its CTest invocation enforces non-silence,
C4 frequency tolerance, DC, peak, step, and stereo activity when a stereo
effect is compiled.

## Automated validation

The following configurations passed on 2026-09-02:

- Windows MSVC Debug and LTO Release: 24/24 tests in each configuration.
- Emscripten Debug and LTO MinSizeRel: 20/20 tests in each configuration.
- Native Tiny profile: 21/21 tests, including all 18 preset IDs and rapid
  switching; its deliberately smaller effect buffers do not reuse the Standard
  acoustic golden.
- Native Standard profile with Chorus, Delay, and Reverb disabled: 21/21 tests.
- Clang 22.1.3 ASan/UBSan: 19/19 tests. The Patch libFuzzer target mutated both
  strict JSON compilation and fixed binary decoding for 20 seconds, including
  successful-parse round trips, without a sanitizer finding.

The instrument integration test renders every preset at C3, C4, C5, C6, and C7,
requires finite non-silent bounded output, and rejects duplicate C4 fingerprints.
It also hard-switches through all 18 presets every 64 frames and limits every
inter-sample transition to less than 0.25.

## Embedded map evidence

ESP-IDF 6.1 rebuilt the current Tiny core, all effects, and all 18 compiled
Patches with warnings as errors for both targets:

| Target | App binary | Core archive | Internal memory result |
|---|---:|---:|---:|
| ESP32 | 146,704 bytes (`0x23d10`) | 20,422 bytes | 147,164 / 180,736 bytes used; 33,572 free |
| ESP32-S3 | 172,560 bytes (`0x2a210`) | 20,646 bytes | 172,655 / 341,760 bytes used; 169,105 free |

The firmware reserves a 131,072-byte static engine arena and performs no audio
callback allocation. These are build and map results only: physical I2S audio,
30-minute underrun/watchdog behavior, and listening quality remain unverified
until suitable boards and audio hardware are available.
