# Golden Update Log

Golden fixtures are changed only after an intentional behavior review. Tests
read these files and never regenerate them.

- 2026-09-02: `m2_event_trace.txt` changed from
  `83658a826364c67e` to `9e6cebee9d02f409`. M3 replaced the fixed 200 ms
  envelope with the selected data-driven preset envelope, so two natural
  Note-Ended event frames moved later. Event count (35), musical ordering,
  transport frame (50,000), and final zero-voice/zero-gesture state remain
  unchanged on Native and Wasm. The conformance render was extended to 100,000
  frames so the 850 ms Grand Piano release completes naturally.
