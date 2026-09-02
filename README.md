# MoL Keyboard · 张多少的键盘

MoL Keyboard (More or Less Zhang's Keyboard) is a lightweight, embeddable,
headless-capable digital instrument system. It turns physical keyboards,
touch, MIDI, GPIO, and programmatic input into real-time performance,
recording, and playback through one portable music engine.

MoL Keyboard（张多少的键盘，英文全称 More or Less Zhang's Keyboard）是一套轻量、
可嵌入、可无界面运行的数字乐器系统。它使用同一个可移植音乐引擎，把物理键盘、触摸、
MIDI、GPIO 和程序事件统一转换为实时演奏、录制与回放。

This repository is a clean-room implementation started from a blank project.
It does not contain code or assets from an earlier MoL Keyboard project.

本仓库是从空白项目开始的全新实现，不包含任何旧版 MoL Keyboard 的代码或资源。

## Current status / 当前状态

Development starts with a freestanding-friendly ISO C11 core. Platform claims
are recorded only after real builds or runtime checks. See
[`docs/status/IMPLEMENTATION_STATUS.md`](docs/status/IMPLEMENTATION_STATUS.md)
and [`docs/status/PLATFORM_MATRIX.md`](docs/status/PLATFORM_MATRIX.md) for current
evidence.

开发首先验证适合裸机环境的 ISO C11 核心。平台支持只在真实构建或运行验证后声明；当前证据
请查看上述状态文档。

## Quick start / 快速开始

The initial native workflow uses CMake, Ninja, and CTest:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

On Windows, run these commands from a Visual Studio developer shell and ensure
Ninja is on `PATH`. Other platform commands will be added only with verified
implementations.

Windows 用户需在 Visual Studio 开发者终端中运行，并确保 `PATH` 中包含 Ninja。其他平台
命令会随真实验证的实现逐步补充。

## Physical key map / 物理键位

The default 30-key chromatic range is C4–F6 and uses physical key codes:

```text
C4–B4: Z S X D C V G B H N J M
C5–B5: Q 2 W 3 E R 5 T 6 Y 7 U
C6–F6: I 9 O 0 P [
Sustain: Space
```

## Headless and Web use / 无界面与 Web 使用

The planned headless entry points are `mol-keyboardd`, `molctl`, and
`mol-render`. The Web product will execute the same C core as WebAssembly in an
AudioWorklet. These products are not claimed as available until their milestone
evidence is recorded.

计划中的无界面入口为 `mol-keyboardd`、`molctl` 和 `mol-render`。Web 产品将在
AudioWorklet 中以 WebAssembly 运行同一份 C 核心；在里程碑证据完成前，不会将这些产品
标记为可用。

## Platform boundaries / 平台边界

- Desktop and mobile Bluetooth audio routing is normally owned by the OS.
- Bluetooth audio latency is measured separately from wired low-latency audio.
- Browser keyboard input requires focus, and audio startup requires a user gesture.
- Mobile background audio follows each platform's service and session rules;
  ordinary hardware keyboard input is not promised while fully backgrounded.
- ESP32-S3 has no Classic Bluetooth A2DP Source capability; original ESP32
  targets may provide it when supported by the SDK and hardware.

- 桌面与移动端蓝牙音频通常由操作系统负责配对和路由。
- 蓝牙音频延迟与有线低延迟音频分开测量。
- 浏览器键盘输入依赖页面焦点，音频启动依赖用户手势。
- 移动端后台音频遵循平台服务与会话规则，不承诺完全后台时接收普通硬件键盘输入。
- ESP32-S3 不支持经典蓝牙 A2DP Source；原始 ESP32 仅在 SDK 与硬件支持时提供。

MoL Keyboard applies conservative digital gain and limiting, but software
cannot guarantee safe acoustic sound pressure because speakers, headphones, and
system volume remain outside the engine's control.

MoL Keyboard 使用保守的数字增益与限幅，但扬声器、耳机和系统音量不受引擎完全控制，
因此软件不能保证实际声压级安全。

## License / 许可证

First-party source code is licensed under Apache-2.0. Assets and examples must
carry their own license records. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).

第一方源代码使用 Apache-2.0 许可证；资源与示例必须分别记录许可证。
