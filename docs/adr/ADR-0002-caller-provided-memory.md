# ADR-0002: Caller-provided engine memory

- Status: Accepted
- Date: 2026-09-02

## Context

Dynamic allocation is unsafe in audio callbacks and may not exist on small
embedded systems. Different profiles also need predictable memory ceilings.

## Decision

The caller asks `mol_engine_query_memory` for a validated configuration,
provides suitably aligned storage, and passes it to `mol_engine_init`. All
variable-capacity engine state is placed inside that arena.

## Consequences

Initialization can fail before audio starts, memory use is map-auditable, and
shutdown has no allocator coupling. Hosts must preserve the arena for the
engine lifetime and must re-query after configuration changes.
