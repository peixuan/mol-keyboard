# Golden Update Log

Golden fixtures are changed only after an intentional behavior review. Tests
read these files and never regenerate them.

To propose an M3 audio update, build `mol_audio_metrics`, inspect the rendered
audio and analyzer report, then capture its stdout explicitly and edit
`m3_audio_metrics.txt` in a reviewed commit. The verifier uses documented
absolute tolerances for peak, RMS, DC, stereo difference, maximum step,
envelope timing, spectral centroid, and three spectral energy bands; it never
writes the fixture.

- 2026-09-02: Added `m3_audio_metrics.txt` after calibrating the fixed 48 kHz,
  C4, velocity 0.8, one-second-gate sequence across all 18 presets. Integrated
  four-second RMS is centered at 0.012 (the lowest preset is 0.011469 and the
  highest is 0.012139), while per-preset peak and spectral identity remain
  distinct. This baseline intentionally follows the first complete procedural
  instrument/effects implementation.

- 2026-09-02: `m2_event_trace.txt` changed from
  `83658a826364c67e` to `9e6cebee9d02f409`. M3 replaced the fixed 200 ms
  envelope with the selected data-driven preset envelope, so two natural
  Note-Ended event frames moved later. Event count (35), musical ordering,
  transport frame (50,000), and final zero-voice/zero-gesture state remain
  unchanged on Native and Wasm. The conformance render was extended to 100,000
  frames so the 850 ms Grand Piano release completes naturally.
