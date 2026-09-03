# Architecture

## Scope

MoL Keyboard is one deterministic instrument engine surrounded by replaceable
hosts. The engine owns music semantics, synthesis, effects, recording, and
transport. Hosts own clocks, devices, permissions, persistence, process
lifecycle, and presentation. No platform header is included by `mol_core`.

## Layers

```text
keyboard / touch / MIDI / GPIO / RPC / program input
                         |
             platform input adapters
                         |
       bounded command queue or direct submission
                         |
    music mapping -> scheduler -> voices -> effects -> limiter
                         |
                  float PCM frames
                         |
 AudioWorklet / miniaudio / Oboe / AudioUnit / OHAudio / I2S
```

- `include/mol` is the versioned public C ABI.
- `src/c_api` owns the arena, command scheduler, state, and render entry points.
- `src/music` and `src/transport` implement platform-independent performance
  semantics and exact frame time.
- `src/dsp`, `src/effects`, and `src/patch` implement the audio signal path and
  data-driven instruments.
- `src/sequence` implements the bounded streaming recording format.
- `platforms` and desktop applications adapt native lifecycle and device APIs.
- `apps/web` is an optional local PWA; it is not required by the engine or
  headless daemon.

## Data and control flow

Every performance source becomes a versioned `mol_command_t` with a source ID,
gesture ID, and target audio frame. `MOL_FRAME_IMMEDIATE` is normalized to the
current frame. Commands are ordered by target frame and submission serial, so
equal-frame behavior is deterministic. Music mapping may expand one gesture
into several owned notes. Processed events feed synthesis and, while recording,
the canonical sequence capture.

The render call advances the engine one frame at a time, applies due commands,
updates transport and arpeggiator state, renders voices, processes stereo
effects, and applies DC blocking, smoothing, and limiting. Hosts convert the
resulting interleaved or planar float PCM only after the core returns.

## Threads and processes

The engine instance has one owner: the audio callback or offline render loop.
Other threads publish fixed-size commands through a bounded SPSC queue owned by
the host. They never call the same engine concurrently. State and events cross
back through bounded copies or versioned snapshots, never an internal pointer.

`mol-keyboardd` separates local IPC, physical input, and device callbacks. Its
dispatcher serializes state-changing RPC work before queueing it for audio.
The Web host constructs the engine inside the AudioWorklet; MessagePort is the
baseline control path and a preallocated SharedArrayBuffer ring is optional.

## Memory

`mol_engine_query_memory` computes one aligned arena size from a validated
configuration. `mol_engine_init` lays every engine object, voice, effect delay,
command heap, event ring, gesture table, and sequence buffer into caller-owned
storage. Shutdown invalidates the engine but never frees that storage.

Tiny, Standard, and Full profiles select compile-time maxima. Runtime counts may
be smaller but cannot exceed the compiled profile. Hosts keep PCM buffers,
device objects, and transport queues outside the engine arena. The render path
does not allocate or resize any container.

## Time

The audio frame is the only realtime scheduling unit. Public frame indices are
64-bit. Tempo conversion carries integer remainders, preventing cumulative
floating-point drift; the two-hour 48 kHz/120 BPM conformance case ends exactly
at frame 345,600,000. Wall clocks are host diagnostics only and do not alter a
deterministic render.

## Persistence and trust boundaries

Patch, sequence, JSON-RPC, MIDI, and Wire input are untrusted. Parsers validate
sizes, versions, counts, numeric ranges, completion records, and checksums
before data reaches the engine. Desktop and mobile settings use private or
user-scoped storage with temporary-file replacement. ESP32 keeps settings in
NVS and sequences in a transactional filesystem store.

Network control is not part of the core. Desktop WebSocket control binds to
loopback and requires an origin plus token. ESP32 Web configuration exists only
in an explicitly authorized SoftAP mode. The standalone Web instrument makes
no application network request after its shell is installed.

## Capability reporting

Capabilities describe the current build and host, not an aspirational matrix.
For example, ESP32-S3 reports USB HID and never Classic Bluetooth A2DP Source;
the original ESP32 reports A2DP only when that implementation is compiled.
Platform support is promoted from source-present to build-, runtime-, and
device-verified only when the corresponding evidence exists.
