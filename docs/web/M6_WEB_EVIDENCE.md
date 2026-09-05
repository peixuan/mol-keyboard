# M6 Web/PWA evidence

Verified on 2026-09-03 and refreshed from an exact clean clone of candidate
`19735a9`. The checked product is the production Vite bundle, not a test-only
page.

## Implemented product paths

- Standards-based TypeScript and Web Components provide the bilingual Explore
  and Studio UI, the complete 30-key instrument, keyboard and independent
  pointer gestures, recording, playback, effects, transport, diagnostics, and
  accessible controls.
- The portable C11 engine runs in an `AudioWorkletProcessor`. The main thread
  fetches the paired Wasm artifact and transfers its bytes through
  `processorOptions`; the processor reports readiness only after asynchronous
  initialization succeeds. Commands use bounded MessagePort batches without
  cross-origin isolation and a preallocated SharedArrayBuffer SPSC ring when
  COOP/COEP are available.
- Settings and local recordings use IndexedDB. The installable PWA caches the
  complete application shell and both generated worklet artifacts for offline
  use.
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
$env:MOL_DAEMON = "..\..\build\dev-release\apps\mol-keyboardd\mol-keyboardd.exe"
npm run test:browser
```

On POSIX hosts, export `MOL_DAEMON` with the corresponding built daemon path.
The Chrome desktop service-controller case fails if that executable is missing;
it is no longer reported as a capability skip.

The production build type-checks and bundles successfully. Its complete output
is 329,232 bytes including the source map. The application entry is 68,388
bytes; the worklet JavaScript is 26,408 bytes and its Wasm is 44,778 bytes. The
release gate counts 158,288 deployable bytes after excluding the source map,
well below the exclusive 2 MiB Web budget.

The exact-candidate browser run executed 42 project/test combinations: 15
applicable paths passed and 27 capability-specific paths were explicitly
skipped. It used the exact Windows Release daemon and verified:

- system Chrome 151.0.7922.175 and system Edge 152.0.4191.53;
- Playwright Firefox 153.0;
- Playwright WebKit 26.5 desktop and iPhone-layout rendering;
- Chromium 151.0.7922.34 with a Pixel 7 layout;
- real realtime AudioWorklet startup and core events in Chrome, Edge, and the
  Chromium mobile layout;
- real AudioWorklet registration, asynchronous Wasm initialization, note
  processing, and non-silent finite DSP output in Firefox through an
  `OfflineAudioContext`;
- zero dropped events while the main thread was deliberately blocked, followed
  by blur release, visibility suspension, and user-gesture resume;
- portrait horizontal key scrolling and a real touch gesture;
- IndexedDB persistence and an offline service-worker reload, followed in
  Chrome by audio startup, note playback, and a core event while offline;
- the non-isolated MessagePort baseline on a server without COOP/COEP;
- a real `mol-keyboardd` subprocess, authenticated control, engine event
  notifications, service-side recording, and rejection of a wrong token.

The same build passed 12/12 Node unit tests, including malformed-Wasm
fail-closed behavior. Emscripten 6.0.5 now passes 46/46 LTO MinSizeRel tests;
the earlier Debug candidate passed 31/31. These include AudioWorklet, event,
sequence, all-preset metric, and fail-closed Web wiring conformance. The
dependency license audit passed after a clean `npm ci` and reports no npm
vulnerability.

The current desktop-first refresh reinstalled the exact lockfile independently
on Windows and in a clean Linux checkout, reporting zero vulnerabilities in
both environments. Node.js 22.16.0 passed 12/12 tests and the production build
on each. System Chrome 151.0.7922.175 on Windows and bundled Chrome for Testing
151.0.7922.34 on Linux each passed all five applicable desktop cases; the two
reported skips are the Firefox-only offline-worklet case and the mobile-layout
case. In both runs the service-controller case spawned the platform's real
`mol-keyboardd`, authenticated over loopback WebSocket, observed engine events,
recorded through the service, rejected a bad token, shut down through local
IPC, and required a clean child-process exit.

Candidate `5e84b49` makes that executable a mandatory browser-acceptance input.
A negative Windows run with an intentionally absent path failed with the
required diagnostic, while real Windows and Linux daemon runs each passed the
service-controller case. A portable CMake audit passes under Windows, Linux,
and Emscripten and rejects restoration of the missing-daemon skip. The Linux CI
job now builds the native daemon, installs the three Playwright browsers, and
runs the full production browser matrix with `MOL_DAEMON` set. That workflow is
checked and YAML-validated locally but remains unexecuted for this unpushed
commit.

The Linux refresh ran the production bundle and the real Linux x86_64 daemon
together under WSL. Playwright's pinned Chromium was selected explicitly so
this result did not depend on a separately installed branded browser:

```sh
export MOL_USE_BUNDLED_CHROMIUM=1
export MOL_DAEMON="$PWD/build/ci-linux-clang/apps/mol-keyboardd/mol-keyboardd"
npm --prefix apps/web run build
node apps/web/scripts/run-browser-tests.mjs test --project=chrome-desktop
```

Five applicable cases passed and two browser-specific cases were skipped. This
is Linux application/service process evidence with a null audio sink; it is not
physical Linux audio, evdev, or macOS Safari evidence.

## Explicit acceptance boundary

Playwright's Windows WebKit port renders the shared UI but does not expose
`AudioWorklet` in this environment. It is not Safari and is not counted as the
required current-stable Safari runtime verification. The headless Firefox
process also exposes no realtime output device, so its actual worklet/Wasm DSP
is proven through offline rendering rather than a live `AudioContext` output.
Actual macOS/iOS Safari, real mobile audio output, and physical-device lifecycle
testing remain open device gates. No Safari or physical-mobile verification is
claimed here.
