# Mol Sequence v1 Format

## Goals

`.molseq` stores canonical post-mapping performance events and the initial music
and effect state needed for deterministic playback. It is little-endian,
forward-only, bounded, streamable, and valid only after a completion record
with a matching CRC32 is present.

## Header

The fixed header is 112 bytes:

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 4 | ASCII `MOLS` magic |
| 4 | 2 | format version 1 |
| 6 | 2 | header size 112 |
| 8 | 4 | recording sample rate, 8,000–192,000 |
| 12 | 4 | nonzero time base, at most 1,000,000 |
| 16 | 4 | zero flags |
| 20 | 4 | initial-state size 80 |
| 24 | 8 | reserved zero |
| 32 | 72 | encoded initial performance state |
| 104 | 8 | reserved zero |

The initial state covers preset, gain, tempo, time signature, octave, transpose,
scale, chord, arpeggiator, sustain, pitch bend, portamento, and metronome. Float
values use little-endian IEEE-754 binary32 and must be finite and in range.

## Records

Each record begins with a one-byte type and a canonical unsigned LEB128 body
length. Body length is at most 512 bytes.

- Type 1 is an event. Its body contains canonical ULEB128 frame delta, command
  type, source ID, gesture ID, payload length, then the command-specific payload.
  Frames are monotonic and the format permits at most 1,000,000 events.
- Type 2 is optional metadata. Its body starts with a little-endian FourCC and
  contains at most 256 metadata bytes.
- Type 255 is the required completion record. It contains canonical ULEB128
  event count and final frame followed by CRC32.

Types with the high bit set may be skipped as future optional records. Zero and
unknown low-bit types are required semantics and fail as unsupported. Event
payload widths and ranges are defined by `mol_sequence_validate_event`; a
decoder re-encodes accepted payloads to enforce canonical form.

## Integrity and streaming

CRC32 covers the header, every record header, and all record data before the
four checksum bytes. It uses polynomial `0xEDB88320`, an all-ones initial state,
and a final complement. The completion count and frame must match observations,
and trailing bytes are forbidden.

Reader callbacks can observe records before the final CRC is known. A caller
that needs transactional delivery validates in one pass and applies in a second,
or stages the bounded result. Writers become inactive after any I/O error and
cannot produce a valid file until finalize succeeds.

## JSON and MIDI tools

`mol-seq` provides strict JSON conversion, validation, inspect, trim, merge,
quantize, and Standard MIDI File type 0/1 conversion. JSON is an interchange and
review format, not the realtime storage contract. MIDI conversion preserves the
supported note, tempo, and controller semantics but cannot represent every MoL
command without loss.

## Conformance

The checked-in `scale-study.molseq` is parsed by Native, Wasm, and both ESP-IDF
startup targets. Binary/JSON and MIDI conversions are deterministic. Corrupt,
truncated, oversized, noncanonical, and incomplete data fail safely, and the
reader/writer and MIDI boundaries are fuzzed.
