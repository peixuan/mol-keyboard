# ADR-0007: ESP32 capability split

- Status: Accepted
- Date: 2026-09-02

## Context

The original ESP32 and ESP32-S3 have different radio, USB, and memory
capabilities. Advertising a uniform feature set would be incorrect.

## Decision

Both targets provide Tiny-profile I2S, GPIO matrix, BLE HID, persistence, and
physical recovery controls. The original ESP32 may provide Classic HID and
A2DP Source/AVRCP. ESP32-S3 provides USB host HID and never reports Classic
Bluetooth A2DP Source. Optional Web configuration uses a larger partition
profile and explicit physical authorization.

## Consequences

Target-specific compile checks and HIL expectations are required. Capability
responses drive UI rather than model-name assumptions. The tight ESP32 Web DRAM
margin makes physical long-run validation mandatory before release.
