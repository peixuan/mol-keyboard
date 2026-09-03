# ADR-0008: Separate sequence and Patch formats

- Status: Accepted
- Date: 2026-09-02

## Context

Instrument definitions and recorded performances have different evolution,
streaming, corruption, and tooling needs.

## Decision

Patches compile from strict JSON into an exact fixed binary with a payload CRC.
Sequences use a fixed initial-state header, length-delimited records, frame
deltas, optional metadata, and a required final CRC/count record.

## Consequences

Embedded presets remain tiny and directly indexable, while recordings remain
forward-streamable and extensible. Both boundaries require explicit versioning,
canonical little-endian encoding, deterministic tools, and parser fuzzing.
