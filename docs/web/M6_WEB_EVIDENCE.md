# M6 Web/PWA evidence

Verified on 2026-09-03 at commit `601f0b7` and the following documentation
commit. The checked product is the production Vite bundle, not a test-only
page.

## Implemented product paths

- Standards-based TypeScript and Web Components provide the bilingual Explore
  and Studio UI, the complete 30-key instrument, keyboard and independent
  pointer gestures, recording, playback, effects, transport, diagnostics, and
  accessible controls.
- The portable C11 engine runs in an `AudioWorkletProcessor`. Commands use
  bounded MessagePort batches without cross-origin isolation and a preallocated
  SharedArrayBuffer SPSC ring when COOP/COEP are available.
- Settings and local recordings use IndexedDB. The installable PWA caches the
  complete application shell and generated worklet module for offline use.
- The optional desktop-controller backend uses an explicitly enabled,
  loopback-only, origin-checked and token-authenticated WebSocket. The token is
  never persisted and is cleared from the UI after a successful connection.
- Audio starts or resumes only from a user gesture. Blur, page hide, hidden
  visibility, pointer cancellation, suspension, and output-device changes are
  handled without leaving owned notes active.

## Reproducible verification

With the pinned Node.js runtime and Playwright browser cache selected:

```powershell
cd apps/web
npm ci
npm run test
npm run test:browser
```

The production build type-checks and bundles successfully. Its complete output
is 302,644 bytes including the source map; the application entry is 60.41 kB
and the generated worklet module is 76.89 kB, below the 2 MiB Web budget.

The browser run executed 36 project/test combinations: 15 applicable paths
passed and 21 capability-specific paths were explicitly skipped. It verified:

- system Chrome 151.0.7922.175 and system Edge 152.0.4191.53;
- Playwright Firefox 153.0;
- Playwright WebKit 26.5 desktop and iPhone-layout rendering;
- Chromium 151.0.7922.34 with a Pixel 7 layout;
- real AudioWorklet startup and core events in Chrome, Edge, Firefox, and the
  Chromium mobile layout;
- zero dropped events while the main thread was deliberately blocked, followed
  by blur release, visibility suspension, and user-gesture resume;
- portrait horizontal key scrolling and a real touch gesture;
- IndexedDB persistence and an offline service-worker reload;
- the non-isolated MessagePort baseline on a server without COOP/COEP;
- a real `mol-keyboardd` subprocess, authenticated control, engine event
  notifications, service-side recording, and rejection of a wrong token.

The same build passed 9/9 Node unit tests. Emscripten 6.0.5 Debug and LTO
MinSizeRel each passed 24/24 CTest tests, including AudioWorklet, event,
sequence, and all-preset metric conformance. The dependency license audit
passed after a clean `npm ci` and reports no npm vulnerability.

## Explicit acceptance boundary

Playwright's Windows WebKit port renders the shared UI but does not expose
`AudioWorklet` in this environment. It is not Safari and is not counted as the
required current-stable Safari runtime verification. Actual macOS/iOS Safari,
real mobile audio output, and physical-device lifecycle testing remain open
device gates. No Safari or physical-mobile verification is claimed here.
