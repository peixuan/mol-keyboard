# Mol Patch v1 Format

## Source document

`.molpatch.json` is a strict UTF-8 JSON object described by
`patches/schema/molpatch-v1.schema.json`. Unknown, missing, duplicate, or
out-of-range fields are errors. The document contains version 1, one of 18
stable IDs, bilingual display names, one of six synthesis model names, one
waveform, and 22 integer parameters. Scaled integer units such as milli,
millidecibel, millihertz, cents, hertz, and milliseconds avoid locale and text
floating-point ambiguity.

`mol-patchc INPUT.json OUTPUT.molpatch` compiles the source. `--c-output` may
also emit an embedded byte array. Compilation is deterministic: equal input
produces equal 120-byte output.

## Binary representation

All integers are little-endian. IEEE-754 layout is not stored because every
payload field is a signed 32-bit integer.

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 4 | ASCII `MOLP` magic |
| 4 | 2 | format version, currently 1 |
| 6 | 2 | header size, exactly 32 |
| 8 | 4 | payload size, exactly 88 |
| 12 | 4 | feature flags |
| 16 | 4 | FNV-1a stable-ID hash |
| 20 | 4 | CRC32 of the 88-byte payload |
| 24 | 8 | zero reserved bytes |
| 32 | 88 | 22 signed 32-bit parameter values |

The payload order is synthesis model, waveform, gain, ADSR, filter cutoff and
resonance, oscillator mix, detune, pulse width, two model parameters, vibrato
rate and depth, velocity curve, noise mix, saturation, and the three effect
sends. `include/mol/patch.h` is authoritative for names and units.

## Feature flags

Flags identify oscillator, filter, noise, FM2, additive, pluck, modal, Chorus,
Delay, and Reverb requirements. A decoder rejects unknown bits and any flag/model
combination that fails `mol_patch_validate`. Disabling an effect at build time
does not change the stored format; the engine reports and handles its compiled
capability.

## Validation and evolution

A decoder requires exactly 120 bytes, canonical reserved zeros, the known
version, known flags, a matching CRC32, and valid parameter relationships. It
never decodes by casting bytes to a C structure. Version 1 has no extension
records; a layout change therefore requires a new format version. All parser
entry points have boundary tests and libFuzzer coverage.

The repository contains the reviewed JSON sources in `patches/builtin`, their
compiled binaries in `patches/compiled`, and generated C arrays under
`src/patch/generated`. Generated results are compared for determinism before a
Patch change is accepted.
