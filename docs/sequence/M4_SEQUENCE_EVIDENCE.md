# M4 Recording and Offline Tool Evidence

Verified on 2026-09-03 at commit `8db93a9`.

## Implemented surface

- Mol Sequence v1 is a forward-only bounded binary format with a versioned
  112-byte header, complete initial state, ULEB128 delta frames, canonical event
  records, optional metadata, CRC32, and a completion footer. Truncation,
  corruption, unsupported critical records, non-monotonic events, oversized
  records, and incomplete writes fail explicitly.
- The engine records processed musical events into caller-owned fixed storage,
  including deterministic gesture correlations and initial effect parameters.
  Playback scales the source time base to the engine sample rate and bypasses a
  second music-mapping pass.
- `mol-seq` implements `inspect`, `validate`, `json-to-binary`,
  `binary-to-json`, `midi-import`, `midi-export`, `trim`, `merge`, and
  `quantize`. JSON is strict and bounded; MIDI SMF 0/1 conversion includes tempo
  maps, time signatures, note correlation, program changes, sustain, pressure,
  and pitch bend.
- `mol-render` accepts `.molseq` or manual-note input and writes PCM16, PCM24, or
  float32 WAV at 8-192 kHz in mono/stereo. High quality runs the engine at 2x
  and downsamples deterministically. Every render reports duration, peak, RMS,
  clipped/non-finite samples, zero offline underruns, and SHA-256 in JSON.

The checked-in example `examples/sequences/scale-study.molseq` is 225 bytes,
contains 12 events ending at frame 108,000, and has SHA-256
`415009bcd666fe8e277a15cb871ee2c3dba2bf73c3b168fcb8ed8b69ddcbb39a`.

## Verification results

| Configuration | Result | Relevant evidence |
|---|---:|---|
| MSVC Debug | 51/51 | format/truncation, record/replay PCM equality, JSON/MIDI/edit operations, three WAV encodings, independent report hash, repeat render |
| MSVC LTO Release | 51/51 | same suite in optimized build |
| Emscripten Debug | 23/23 | shared fixture and recording/sequence tests run through Node |
| Emscripten LTO MinSizeRel | 23/23 | same optimized Wasm suite |
| Clang ASan/UBSan | 23/23 | all portable tests plus 20-second Patch and Sequence libFuzzer sessions |
| ESP32 | build passed | shared fixture parser runs before I2S startup; 153,440-byte app binary |
| ESP32-S3 | build passed | same startup parser; 179,328-byte app binary |

Native and Wasm execute the same bytes generated directly from the checked-in
example and match:

```text
sample_rate=48000 time_base=48000 events=12 metadata=1 note_on=4 note_off=4 final=108000
```

The ESP firmware embeds bytes generated from that same file and rejects startup
if its parser summary differs. ESP claims remain build-verified because no
physical board was connected for this run.
