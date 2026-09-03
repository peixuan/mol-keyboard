# MolWireEventV1 Protocol

MolWireEventV1 carries latency-sensitive performance input over a transport
chosen by the host, such as WebSocket binary, serial, BLE GATT, or a test pipe.
It is not the desktop service RPC protocol.

Every packet is exactly 48 bytes and uses little-endian integers and IEEE-754
binary32 values:

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 4 | ASCII `MOLW` magic |
| 4 | 2 | format version 1 |
| 6 | 2 | event type |
| 8 | 2 | packet size 48 |
| 10 | 2 | flags, zero in v1 |
| 12 | 4 | sender sequence number |
| 16 | 8 | target audio-frame timestamp |
| 24 | 4 | source ID |
| 28 | 4 | reserved zero |
| 32 | 8 | gesture ID |
| 40 | 8 | type-specific payload |

Types are note-on, note-off, control, pitch bend, all-notes-off, and
all-sound-off. Note payload is note byte, three zero bytes, and velocity.
Control payload is a 16-bit control ID, two zero bytes, and value. Pitch bend is
a float followed by four zero bytes. Empty event types require eight zero bytes.

Velocity and supported controls use 0–1; pitch bend uses -1–1; all values must
be finite. Sustain and master gain translate to core commands. Modulation is
valid on the wire but currently reports unsupported when translated because the
core has no matching public command.

The packet sequence number lets a transport detect loss or duplication; the
core does not reorder it. Timestamp becomes `mol_command_t.target_frame`, so a
bridge must establish the frame-clock relationship. Authentication, replay
policy, framing, retry, and confidentiality belong to the enclosing transport.

Decoding validates exact length, magic, version, flags, reserved bytes, type,
and value ranges. Bytes are decoded explicitly rather than copied into a native
structure. Unit, corruption, round-trip, and libFuzzer tests cover the boundary.
