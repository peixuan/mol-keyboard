# ADR-0001: ISO C11 portable core

- Status: Accepted
- Date: 2026-09-02

## Context

The same musical behavior must run on desktops, WebAssembly, mobile native
hosts, and ESP-IDF without maintaining divergent engines.

## Decision

All engine, music, DSP, Patch, sequence, transport, and Wire behavior is ISO
C11 with no platform header. The public boundary is C ABI. C++17 is limited to
host and tooling code.

## Consequences

One source set is conformance-tested across Native, Emscripten, and ESP-IDF.
Platform facilities enter only through host-owned buffers and commands. The
core cannot depend on exceptions, RTTI, OS services, or managed runtimes.
