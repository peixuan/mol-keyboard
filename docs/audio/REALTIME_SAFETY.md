# Realtime Safety

## Realtime boundary

The realtime boundary begins when a platform audio callback acquires its output
buffer and ends when that buffer is returned. `mol_engine_render_interleaved_f32`
and `mol_engine_render_planar_f32` are designed for that boundary. The same
rules apply to the Web AudioWorklet `process()` function and the ESP32 I2S task.

## Allowed work

- Read and write fixed caller-owned memory.
- Pop bounded fixed-size commands without waiting.
- Update deterministic engine, voice, transport, and effect state.
- Render a bounded number of frames and voices.
- Copy fixed-size events into a bounded event ring.
- Perform finite arithmetic with validated and smoothed parameters.
- Clear a preallocated buffer on a recovery or all-sound-off path.

## Forbidden work

- Heap allocation, deallocation, container growth, or lazy initialization.
- File, network, console, logging, or preference I/O.
- Mutex, condition-variable, semaphore, process, or unbounded atomic waits.
- Device enumeration, route selection, thread creation, or teardown.
- Exceptions or callbacks into a managed JavaScript, JVM, Swift, or ArkTS
  runtime.
- Parsing JSON, MIDI, Patch text, or a sequence stream.
- Work whose bound depends on an untrusted count.

The core itself is single-owner rather than internally locked. Calling submit,
render, reset, or state-mutating APIs concurrently on one engine is invalid.
Platform hosts use a preallocated queue or serialize direct calls on the audio
owner thread.

## Bounded queues and overload

Queue capacity is selected before start. A producer that reaches capacity gets
an explicit error and increments diagnostics; it does not block the callback or
overwrite an unrelated gesture. Event polling is likewise capacity-bounded.
An emergency `ALL_SOUND_OFF` path terminates active sound with a finite ramp.

## Setup and teardown

Parsing, arena sizing, device creation, permission requests, route discovery,
and persistent-state loading occur before the callback begins. Device loss is
reported to a control thread. That thread stops and rebuilds the device around
the same bounded runtime; the callback never reconstructs a backend itself.

Shutdown first prevents new input, submits release/all-sound-off behavior where
the backend permits it, stops the device callback, and only then invalidates
the arena. Caller storage is not freed by `mol_engine_shutdown`.

## Numeric safety

Patch and command validators reject non-finite or out-of-range parameters.
Oscillator, filter, feedback, envelope, and effect coefficients are clamped to
stable ranges. Master gain and preset changes are smoothed, DC blockers remove
offset, and the final limiter uses a conservative ceiling. Every conformance
render scans for NaN/Inf and out-of-range output.

## Evidence

The implementation is checked by source-level static analysis, allocation-free
arena tests, queue wrap and pressure tests, ThreadSanitizer on Linux, ASan/UBSan,
randomized property tests, recovery cycling, and an optimized 30-minute
simulated render. Physical callback deadline and underrun claims still require
the target device, route, and capture report; a faster-than-realtime simulation
does not replace that measurement.
