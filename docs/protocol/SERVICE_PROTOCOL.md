# Desktop Service Protocol

## Transports

The default transport is local-only: a per-user Windows named pipe or POSIX
Unix-domain socket. One request and response are each limited to 65,536 bytes.
The optional browser endpoint binds IPv4 loopback, accepts WebSocket text frames
only, requires an exact Origin allowlist and a fresh 256-bit session token, and
uses the same message limit. It rejects invalid masking, UTF-8, fragmentation,
binary frames, remote peers, tokens, and origins.

## JSON-RPC profile

Messages use JSON-RPC 2.0. Requests contain only `jsonrpc`, `method`, optional
object/array `params`, and optional string/number/null `id`. A missing ID is a
notification and has no response. Empty or oversized batches are rejected;
batch size, nesting, string, member, method, and overall request sizes are
bounded by the strict parser.

Standard errors include parse error `-32700`, invalid request `-32600`, method
not found `-32601`, and internal error `-32603`. Domain validation errors use a
stable `RpcError` code and a non-sensitive message. Unknown request members and
unknown per-method parameters fail rather than being ignored.

## Methods

The v1 service registers exactly these groups:

- System: `system.getInfo`, `system.getCapabilities`, `system.getMetrics`,
  `system.shutdown`.
- Engine: `engine.getState`, `engine.reset`, `engine.allNotesOff`,
  `engine.allSoundOff`.
- Preset: `preset.list`, `preset.select`, `preset.getParameters`,
  `preset.setParameter`.
- Transport: `transport.get`, `transport.setTempo`,
  `transport.setTimeSignature`, `transport.start`, `transport.stop`.
- Input: `input.listDevices`, `input.attach`, `input.detach`,
  `input.getMapping`, `input.setMapping`.
- Audio: `audio.listDevices`, `audio.selectDevice`, `audio.getLatency`.
- Performance: `performance.noteOn`, `performance.noteOff`,
  `performance.control`.
- Recording: `recording.start`, `recording.stop`, `recording.list`,
  `recording.load`, `recording.save`.
- Playback: `playback.start`, `playback.stop`, `playback.seek`.
- Configuration: `config.get`, `config.set`.
- Diagnostics: `diagnostics.selfTest`, `diagnostics.doctor`,
  `diagnostics.benchmark`.

`engine.events` is a server notification on the WebSocket transport. Its
`events` array contains bounded copies from the engine event queue; the network
thread never calls from the audio callback.

## State and lifecycle

Mutations are serialized by the daemon dispatcher and submitted to the audio
owner through its bounded queue. Configuration writes validate an allowlist and
use temporary-file replacement. `system.shutdown`, process signals, and console
stop events release notes, stop input and audio, close endpoints, and remove a
Unix socket. Exact parameters and response fields are exercised by
`mol_service_backend_tests`; clients should use capability responses instead of
assuming an output route or Bluetooth feature.
