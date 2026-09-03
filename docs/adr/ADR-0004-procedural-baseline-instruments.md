# ADR-0004: Procedural baseline instruments

- Status: Accepted
- Date: 2026-09-02

## Context

Every target needs 18 useful presets, but sample libraries increase storage,
licensing, download, and embedded-memory cost.

## Decision

The baseline uses strict data-driven Patches over six procedural synthesis
models. All 18 presets compile to fixed 120-byte binaries embedded in the core.
Sampling remains an optional non-Tiny extension.

## Consequences

All targets expose the same preset identifiers without external assets. Patch
quality is constrained by the portable DSP budget and therefore needs metric,
listening, and device validation rather than sample provenance management.
