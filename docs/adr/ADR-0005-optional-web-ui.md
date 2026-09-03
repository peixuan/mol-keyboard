# ADR-0005: Optional Web UI

- Status: Accepted
- Date: 2026-09-02

## Context

The product needs an accessible instrument surface, but headless services and
embedded targets must not acquire a browser or networking dependency.

## Decision

The production UI is a separately built local PWA. It runs the core in an
AudioWorklet for standalone use and may control the daemon through an explicit
authenticated loopback WebSocket. Native hosts can package the same static
assets behind offline-only bridges.

## Consequences

The daemon, CLI, and core remain usable without UI. Web assets are optional in
normal installs and explicitly enabled for release packages. Browser lifecycle,
autoplay, isolation, and offline behavior require their own acceptance matrix.
