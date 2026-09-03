# ADR-0006: Platform-native audio hosts

- Status: Accepted
- Date: 2026-09-02

## Context

One cross-platform wrapper cannot expose every platform's low-latency,
background, interruption, and route behavior accurately.

## Decision

Use miniaudio device I/O on desktop, AudioWorklet on Web, Oboe/AAudio on
Android, AudioUnit/AVAudioSession on Apple, OHAudio on HarmonyOS, and I2S on
ESP-IDF. Each host calls the same C engine and reports its effective settings.

## Consequences

Music and DSP remain identical while lifecycle code stays platform-specific.
Each host needs real SDK and device evidence. Bluetooth output on OS platforms
is normally a system route; its latency is measured separately.
