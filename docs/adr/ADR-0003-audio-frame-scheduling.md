# ADR-0003: Audio-frame scheduling

- Status: Accepted
- Date: 2026-09-02

## Context

Wall-clock and block-relative scheduling changes with callback size and creates
cross-platform timing drift.

## Decision

Commands use an absolute unsigned 64-bit target frame. Immediate commands are
normalized at submission. Equal-frame commands retain submission order, and
tempo conversion carries an integer remainder.

## Consequences

Rendering is sample-accurate and deterministic across block sizes. Hosts must
translate external timestamps to engine frames and treat device latency as a
separate measurement.
