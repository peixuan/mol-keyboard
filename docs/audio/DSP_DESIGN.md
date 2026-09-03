# DSP Design

## Signal graph

Each active voice renders a procedural model, applies its amplitude and timbre
controls, and sends a dry stereo signal plus bounded amounts to Chorus, Delay,
and Reverb. The master path smooths gain, removes DC, limits peaks, and applies
a short transition ramp after disruptive state changes.

```text
model -> oscillator/noise -> state-variable filter -> ADSR -> saturation
                                                           | dry
                                                           + chorus send
                                                           + delay send
                                                           + reverb send
     stereo sum -> smoothed gain -> DC blocker -> limiter -> output ramp
```

## Synthesis models

- Subtractive combines band-limited sine, saw, square, pulse, triangle, or
  deterministic noise with a stable state-variable filter.
- FM2 uses a sine modulator and carrier; Patch model parameters set ratio and
  modulation index.
- Additive sums a bounded harmonic set with normalized gain.
- Pluck uses a fixed-capacity Karplus-Strong delay selected by the build profile.
- Modal uses damped resonant modes for struck and string-like instruments.
- Formant combines stable resonances for choir and wind-like spectra.

Every voice snapshots its Patch at note start. A graceful preset switch affects
new notes while existing releases keep their old timbre. A hard switch uses the
master transition ramp and clears effect state where needed.

## Modulation and envelopes

ADSR times are converted to per-frame coefficients at setup or state change.
Zero attack and decay are supported without division by zero. Velocity follows
the Patch curve before voice gain. Vibrato is a bounded low-frequency phase
modulation. The public 0--1 modulation parameter adds at most 50 cents of
vibrato depth, providing a deterministic target for MIDI CC1. Pitch bend,
portamento, octave, transpose, scale, and chord mapping are resolved through the
common music state rather than platform code.

## Effects

Chorus uses a modulated fractional delay. Delay uses bounded stereo feedback
storage. Reverb uses a lightweight fixed comb/all-pass network with profile
specific buffer lengths. Feedback and damping are clamped below unstable
ranges, sends and mixes are smoothed, and no effect allocates while rendering.
Compile-time switches can remove any effect while preserving the public API.

## Profiles and determinism

Tiny, Standard, and Full change maxima and delay-line storage, not command or
preset identifiers. Noise, random arpeggiation, and voice decisions use explicit
seeds. Given the same profile, configuration, command stream, and sample rate,
fresh runs produce the same event sequence and PCM within documented
cross-architecture tolerances.

## Output safety

Parameters reject NaN/Inf before entering state. Coefficients and feedback are
bounded, denormal-prone tails are cleared, master gain is smoothed, and the
limiter ceiling defaults near -1 dBFS. This protects digital output but cannot
guarantee acoustic sound pressure, which also depends on the host volume,
amplifier, transducer, and listener distance.

## Validation

DSP primitives have impulse, stability, finite-output, response, and property
tests. All 18 presets render five octaves, must be non-silent and distinguishable,
and pass the shared Native/Wasm metric golden. Rapid preset switching has a
maximum-step gate. ASan/UBSan, static analysis, and the endurance render add
memory, numeric, and long-run coverage. See `M3_AUDIO_EVIDENCE.md` for the
calibration record.
