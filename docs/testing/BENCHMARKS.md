# Benchmarks and Size Evidence

## Scope and host

Unless a row says otherwise, current local M10 measurements were made on
2026-09-03 on Windows 11 Pro for Workstations 10.0.26200 with an Intel Xeon
W-2295 (18 cores/36 threads) and 64-bit MSVC 19.51.36248. WSL results use Ubuntu
on Linux 6.18.33.2 and the same CPU. Results are engineering evidence for this
host, not universal device claims.

## Optimized endurance

The Standard Release engine rendered 1,800 seconds of 48 kHz stereo audio:

| Metric | Result |
| --- | ---: |
| Rendered frames | 86,400,000 |
| Configured / active gestures | 32 voices / up to 16 |
| CPU wall time | 268.567 s |
| Simulated/realtime ratio | 6.70x |
| Approximate one-core utilization | 14.92% |
| Random note-ons | 57,600 |
| Preset switches | 360 (20 complete rotations) |
| Recording cycles | 120 |
| Near-full 256-entry queue cycles | 29 |
| Emitted events | 230,136 |
| Peak / non-finite samples | 0.891251 / 0 |

The ratio exceeds the 4x gate corresponding to an average below 25% of one
core on this host. This is a single-process simulation without a physical audio
driver; it does not establish callback P95, underrun, or device thermal results.

The null-backend runtime recovery test performed 30 full device stop/rebuild
cycles in 1.42 seconds with zero render
failures. Exact transport arithmetic reached frame 345,600,000 after the
two-hour conformance case with zero drift.

## Desktop diagnostic

An independent Windows WASAPI service run rendered the 96,000-frame diagnostic
at 80.68x realtime with zero non-finite samples. It used the active 48 kHz
stereo system output. The host exposed no Bluetooth route, so no Bluetooth
playback or latency conclusion is drawn from that run.

## Release size gate

The reproducible Linux Release gate used `/usr/bin/llvm-strip-21` on a copied
archive and deterministic gzip level 9 for Wasm:

| Measurement | Actual | Exclusive limit | Result |
| --- | ---: | ---: | --- |
| Stripped `mol_core` archive | 505,956 B | 1,048,576 B | pass |
| Headless daemon + CLI | 976,136 B | 5,242,880 B | pass |
| Compressed Standard Wasm | 23,018 B | 1,572,864 B | pass |
| Web core resources, excluding maps | 157,610 B | 2,097,152 B | pass |

The Web total includes HTML, JavaScript, CSS, manifest, service worker, icons,
and the paired AudioWorklet JavaScript and Wasm artifacts. It excludes only
source maps and contains no optional samples.

Audited CPack outputs from code candidate `4f77f56` contained 146 files each,
including both worklet artifacts, the complete C SDK/export header, the latency
probe, and its physical measurement procedure. The Windows AMD64 ZIP was
1,291,580 bytes with SHA-256
`0d419cce06880e24ca871548bbbbe8f1a4ef0f59c38f56bd75baf2157907b7ab`.
The Linux x86_64 TGZ was 1,676,870 bytes with SHA-256
`255bc2069c9d33b8506f2d03d9d8732a26295308ebdb6dd7862202e95e6b5492`.
Both installed daemon/CLI smoke tests passed, and the audit requires the latency
probe executable. These are unsigned local 0.1.0 pre-release artifacts, not
v1.0.0 releases.

## ESP-IDF build budgets

ESP-IDF 6.1/GNU 15.2.0 produced these build/map results:

| Target | Default image | Web image | Default internal use | Web internal use |
| --- | ---: | ---: | ---: | ---: |
| ESP32 | 1,018,256 B | 1,551,168 B | 101,892 / 124,580 B DRAM | 118,000 / 124,580 B DRAM |
| ESP32-S3 | 796,832 B | 1,302,192 B | 148,923 / 341,760 B DIRAM | 187,975 / 341,760 B DIRAM |

The Tiny core archive remains below 28 KiB. Its eight-voice engine query is
37,664 bytes in a 37,888-byte arena. The ESP32 Web configuration leaves only
6,580 bytes of reported static DRAM margin, so physical long-run HIL remains a
mandatory release gate.

## End-to-end latency

The installed `mol-latency-probe` has five passing native tests and a seventh
sanitizer-backed fuzz boundary. Its deterministic 20-event analyzer fixture
reports P50 19.5 ms, P95 28.05 ms, and maximum 29 ms; the tests also verify
threshold failure and corrupt-container rejection. These are analyzer results,
not product latency.

No physical loopback/capture instrument was available. Consequently all
required product latency rows remain open rather than receiving estimated
values:

| Route | P50 | P95 | Maximum | State |
| --- | ---: | ---: | ---: | --- |
| Representative built-in/wired | — | — | — | physical measurement required |
| USB audio | — | — | — | physical measurement required |
| System Bluetooth audio | — | — | — | physical measurement required |
| Desktop Web Audio | — | — | — | physical stimulus/capture required |
| ESP32 I2S | — | — | — | board and capture required |
| ESP32 A2DP | — | — | — | original ESP32 and sink required |

The release report must add stimulus method, hardware models, route, effective
sample rate and buffer, run count, distribution, and artifact commit before
these rows can close. Use the synchronized two-channel, fail-closed procedure
and exact probe invocation in `docs/testing/LATENCY_MEASUREMENT.md`.
