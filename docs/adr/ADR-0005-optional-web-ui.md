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

The desktop application embeds those assets in a wxWidgets 3.2 shell. It uses
the operating system WebView backend (WebView2 on Windows, WebKit2GTK on Linux,
and WKWebView on macOS) and a random loopback-only HTTP origin so the existing
cross-origin-isolated AudioWorklet/Wasm path remains the only production UI
implementation. The shell exposes no JavaScript-to-native bridge, disables
developer tools and popups, and rejects navigation outside its exact local
origin.

A separately enabled wxWidgets native debugger may connect to the headless
service over the same bounded local IPC used by `molctl`. It is an operational
diagnostic client, not a second production audio owner.

## Consequences

The daemon, CLI, and core remain usable without UI. Web assets are optional in
normal installs and explicitly enabled for release packages. Browser lifecycle,
autoplay, isolation, and offline behavior require their own acceptance matrix.
The desktop GUI adds a checksum-locked wxWidgets build dependency and a system
WebView runtime dependency. Linux builders need GTK3 and WebKit2GTK development
packages. The native debugger remains optional for ordinary builds. Portable
release presets explicitly enable and package it so extracted-package
acceptance can verify service control independently of both the production
WebView instrument and the no-UI CLI path.
