# Desktop headless service

`mol-keyboardd` runs in the foreground as the current user and does not open a
window, Web server, or LAN listener. It uses a Windows Named Pipe or POSIX Unix
domain socket for local JSON-RPC. Start it manually with:

```sh
mol-keyboardd
molctl status
```

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

The service releases active notes, closes physical-input adapters, stops the
audio device, and removes its Unix socket on RPC shutdown, SIGINT/SIGTERM, or a
Windows console stop event.
