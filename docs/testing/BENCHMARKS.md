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
| CPU wall time | 335.666 s |
| Simulated/realtime ratio | 5.36x |
| Approximate one-core utilization | 18.66% |
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
cycles, 580 callbacks, and 74,240 frames with zero render failures. Exact
transport arithmetic reached frame 345,600,000 after the two-hour conformance
case with zero drift.

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
| Stripped `mol_core` archive | 510,092 B | 1,048,576 B | pass |
| Headless daemon + CLI | 947,488 B | 5,242,880 B | pass |
| Compressed Standard Wasm | 22,953 B | 1,572,864 B | pass |
| Web core resources, excluding maps | 162,708 B | 2,097,152 B | pass |

The Web total includes HTML, JavaScript, CSS, manifest, service worker, icons,
and the single-file AudioWorklet/Wasm module. It excludes only source maps and
contains no optional samples.

Audited CPack outputs contained 117 files each. The Windows AMD64 ZIP was
935,972 bytes with SHA-256
`667760b940423eb79d8e07e79f9c34cfd9d17155d1843ed0bcbe3b7d394ccb17`.
The Linux x86_64 TGZ was 1,345,584 bytes with SHA-256
`3171f2fbf38296d17ca1b1cf0197d11c3974f640bd0c77308e0c7efeec2a2930`.
These hashes identify local 0.1.0 pre-release artifacts, not v1.0.0 releases.

## ESP-IDF build budgets

ESP-IDF 6.1/GNU 15.2.0 produced these build/map results:

| Target | Default image | Web image | Default internal use | Web internal use |
| --- | ---: | ---: | ---: | ---: |
| ESP32 | 1,018,096 B | 1,550,992 B | 101,892 / 124,580 B DRAM | 117,984 / 124,580 B DRAM |
| ESP32-S3 | 796,656 B | 1,302,032 B | 148,923 / 341,760 B DIRAM | 187,975 / 341,760 B DIRAM |

The Tiny core archive remains below 28 KiB. Its eight-voice engine query is
37,664 bytes in a 37,888-byte arena. The ESP32 Web configuration leaves only
6,596 bytes of reported static DRAM margin, so physical long-run HIL remains a
mandatory release gate.

## End-to-end latency

No physical loopback/capture instrument was available. Consequently all
required latency rows remain open rather than receiving estimated values:

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
these rows can close.
