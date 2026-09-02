# Desktop Audio Host

`mol-play` is the minimal headless realtime desktop host. It sends the exact
`mol_core` float output directly to miniaudio's device callback and supports:

- WASAPI on Windows;
- Core Audio on macOS;
- PulseAudio, ALSA, or JACK on Linux;
- a deterministic null backend for automated callback testing.

The Windows, macOS, and Linux hosts use the same C++17 source and public C ABI.
miniaudio is pinned and audited in `third_party/manifest.lock.json`; only its
low-level device I/O is compiled.

## Device selection

With no device option, the host follows the operating-system default output:

```powershell
mol-play --duration 2 --note 60 --velocity 0.5
```

List outputs with stable backend identifiers:

```powershell
mol-play --list-devices
mol-play --device-id <complete-hex-id> --duration 2
```

The identifier is the pinned miniaudio backend's complete opaque
`ma_device_id` encoded as hexadecimal and is compared by that backend on the
next run. Bluetooth speakers appear only when the operating system exposes
them as playback devices; pairing remains an operating-system responsibility.

The default-device path allows backend automatic rerouting. The notification
callback records starts, stops, and reroutes. If playback stops externally, the
host does not restart from the callback: it leaves output silent, reports the
loss on the control thread, and exits with status 2.

## Callback contract

The host requests a 128-frame, three-period, low-latency shared stream but uses
the actual sample rate and accepts variable callback sizes. It prints the
backend's native period geometry and a buffer-duration estimate at startup.
The callback renders directly into the supplied stereo float buffer. It does
not allocate, lock, log, access files, or submit commands. Render failure or a
non-finite sample is replaced with silence and counted.

The null-backend CTest runs for at least one second and validates callback
activity, C4 frequency, peak, render failures, and finite output. This checks
the realtime code path without claiming physical playback.

## Windows runtime evidence

On 2026-09-02 the Release executable opened the real default WASAPI device
`Realtek HD Audio 2nd output (Realtek High Definition Audio)` at 48 kHz. It
negotiated 480 frames and three native periods (30 ms aggregate buffer-duration
estimate), delivered 109 callbacks and 52,320 frames, measured C4 at 261.25 Hz,
and reported peak 0.06128063 with zero render failures and non-finite samples.

That software buffer estimate is not an end-to-end acoustic latency
measurement. P95 wired, USB, Bluetooth, and route-change latency still require
the later hardware measurement gate.
