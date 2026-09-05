# Desktop GUI

MoL Keyboard has two wxWidgets desktop applications with deliberately separate
roles:

- `mol-keyboard` is the production instrument. A native frame embeds the complete
  local Web/PWA UI through WebView2 on Windows, WebKit2GTK on Linux, or WKWebView
  on macOS. Standalone AudioWorklet/Wasm synthesis does not require the daemon.
- `mol-keyboard-debug` is an optional fully native service debugger. Its native
  controls exercise presets, tempo, velocity, notes, sustain, recording, audio
  enumeration, diagnostics, and raw response logging through local IPC. It does
  not render audio itself.

## Build and test

Both applications use checksum-locked wxWidgets 3.2.11 source. Windows also
uses the checksum-locked WebView2 loader SDK; the Evergreen WebView2 runtime is
the system rendering engine. Linux needs GTK3, WebKit2GTK 4.1, and Xvfb for
headless acceptance:

```bash
sudo apt-get install --no-install-recommends libgtk-3-dev libwebkit2gtk-4.1-dev xvfb xauth
```

Configure both applications without changing the normal headless build:

```bash
cmake -S . -B build/desktop-gui -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMOL_BUILD_DESKTOP_GUI=ON -DMOL_BUILD_NATIVE_DEBUG_GUI=ON \
  -DMOL_BUILD_TESTS=ON
cmake --build build/desktop-gui
ctest --test-dir build/desktop-gui --output-on-failure \
  -R 'mol_(desktop_(web_server|app_support|webview)|native_debug)'
```

The WebView acceptance loads the production bundle and verifies AudioContext,
AudioWorklet, cross-origin isolation, and IndexedDB in the real native backend.
It records whether the platform selected the SharedArrayBuffer fast path or the
supported MessagePort fallback. The native debugger acceptance constructs the
real window, starts a real `mol-keyboardd --null-backend`, queries state and
capabilities, runs self-test, sends note-on/note-off, silences the engine, and
requests a clean service shutdown. Linux runs both windows inside Xvfb when no
display is present.

## Run

Start the production instrument directly:

```text
mol-keyboard [--web-root PATH]
```

For the native debugger, start the per-user service first and then the debugger:

```text
mol-keyboardd
mol-keyboard-debug [--state-dir PATH] [--endpoint LOCAL_ENDPOINT]
```

Use `mol-keyboardd --null-backend` when no audio device is available. Closing
either GUI does not stop an independently running service.

## Security and ownership boundaries

The production shell binds an ephemeral port on `127.0.0.1`, serves only the
validated local bundle, and stops that server with the window. It accepts only
its exact generated origin plus the initial `about:blank`, blocks new windows,
and exposes no privileged script message handler. Context menus, history, and
developer-tool access are disabled.

The native debugger uses the daemon's existing local named pipe or Unix-domain
socket. The OS local-user boundary, 64 KiB IPC message limit, strict JSON-RPC
schemas, and daemon-side validation remain authoritative. Neither GUI is linked
into the portable core, and `MOL_BUILD_DESKTOP_GUI=OFF` keeps headless builds
free of wxWidgets and WebView dependencies.
