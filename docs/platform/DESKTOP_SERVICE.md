# Desktop headless service

`mol-keyboardd` runs in the foreground as the current user and does not open a
window, Web server, or LAN listener by default. It uses a Windows Named Pipe or
POSIX Unix domain socket for local JSON-RPC. Start it manually with:

```sh
mol-keyboardd
molctl status
```

## Browser controller

The optional browser control endpoint must be enabled explicitly and only binds
the IPv4 loopback interface. Every allowed Web origin is an exact match; no
wildcards are accepted. For the Vite development server, run:

```sh
mol-keyboardd --websocket-port 8766 --web-origin http://127.0.0.1:4173
```

The service prints a `ws://127.0.0.1:8766/control` endpoint and a fresh 256-bit
session token after startup. Enter both in the Web UI. The UI clears the token
field after a successful connection and never persists it. Restart the service
to rotate the token. Do not put the token in source files, shell history, URLs
that are shared with other people, or diagnostic reports.

The server checks the peer address, exact `Origin`, token, WebSocket masking and
UTF-8, and applies the same 64 KiB bound as local IPC. It accepts JSON-RPC text
frames only. Invalid binary, fragmented, oversized, or malformed frames are
closed. Core events are copied out of the audio callback through a fixed-size
SPSC queue and delivered as bounded `engine.events` notifications; the network
thread never runs in the audio callback.

Use `--null-backend` for service, CI, and diagnostics on a machine without an
audio device. A paired Bluetooth speaker works when the operating system exposes
it as a normal playback device; the desktop service does not implement A2DP.

## User-level startup

Linux users may copy `mol-keyboardd.service` to
`~/.config/systemd/user/`, adjust `ExecStart` if the executable is not installed
at `~/.local/bin/mol-keyboardd`, then run:

```sh
systemctl --user daemon-reload
systemctl --user enable --now mol-keyboardd.service
```

Developers with a running systemd user manager can validate the shipped unit
policy and real background lifecycle without installing a persistent unit:

```sh
platforms/linux/run-systemd-user-smoke.sh \
  build/ci-linux-gcc/apps/mol-keyboardd/mol-keyboardd \
  build/ci-linux-gcc/apps/molctl/molctl
```

The runner uses a unique runtime-only user unit, null audio, private state and
IPC paths, and removes the link after the service exits successfully.

macOS users may copy `cn.zhangpeixuan.molkeyboard.daemon.plist` to
`~/Library/LaunchAgents/`, adjust `/usr/local/bin/mol-keyboardd` when needed, and
run:

```sh
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/cn.zhangpeixuan.molkeyboard.daemon.plist
```

Windows users may run `install-user-startup.ps1 -Executable PATH` from the
installed service directory. It creates a shortcut only in the current user's
Startup folder and does not request administrator access. Run
`uninstall-user-startup.ps1` to remove that shortcut.

The Windows acceptance runner creates the same WScript shortcut in a temporary
Startup-directory model, validates and launches it, exercises the real daemon
and CLI, then uninstalls it without touching the real Startup folder:

```powershell
platforms/windows/run-startup-smoke.ps1 `
  -Daemon build/dev-release/apps/mol-keyboardd/mol-keyboardd.exe `
  -Controller build/dev-release/apps/molctl/molctl.exe
```

The service releases active notes, closes physical-input adapters, stops the
audio device, and removes its Unix socket on RPC shutdown, SIGINT/SIGTERM, or a
Windows console stop event.
