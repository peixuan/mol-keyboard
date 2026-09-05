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

The portable core, music/DSP, recording/tooling, desktop, Web/PWA, Android,
iOS, HarmonyOS, and ESP32 implementations are present. Native, Wasm, Android
emulator, Linux AArch64 QEMU execution, all four ESP-IDF builds, and real
ESP32/ESP32-S3 firmware execution in Espressif QEMU have current evidence;
Apple, DevEco, physical mobile/ESP32 hardware, Safari, and measured end-to-end
latency remain release blockers. This repository is therefore a 0.1.0
prerelease and is not tagged v1.0.0. Platform claims are recorded only after
real builds or runtime checks. See
[`docs/status/IMPLEMENTATION_STATUS.md`](docs/status/IMPLEMENTATION_STATUS.md)
and [`docs/status/PLATFORM_MATRIX.md`](docs/status/PLATFORM_MATRIX.md) for current
evidence.

可移植核心、音乐与 DSP、录音工具、桌面、Web/PWA、Android、iOS、HarmonyOS 和
ESP32 实现均已落地。Native、Wasm、Android 模拟器、Linux AArch64 QEMU 执行、四个
ESP-IDF 构建以及 ESP32/ESP32-S3 真实固件在 Espressif QEMU 中的执行均有当前证据；
Apple、DevEco、移动与 ESP32 真机、Safari 以及真实端到端延迟仍是发布门禁。因此当前版本
仍为 0.1.0 预发布版，不标记为 v1.0.0。平台支持只在真实构建或运行验证后声明。

## Quick start / 快速开始

The initial native workflow uses CMake, Ninja, and CTest:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

On Windows, run these commands from a Visual Studio developer shell and ensure
Ninja is on `PATH`. Linux uses the same presets with GCC or Clang. Native test
presets also require Node.js for the exact HarmonyOS production-policy test;
set `EMSDK_NODE` to the executable when `node` is not on `PATH`. Configuration
also requires a Python 3 interpreter for the HIL/QEMU evidence parsers and
Linux launchd-process simulation. Missing either runtime fails configuration
rather than silently omitting tests.

Windows 用户需在 Visual Studio 开发者终端中运行，并确保 `PATH` 中包含 Ninja；Linux
可用同一组预设配合 GCC 或 Clang。原生测试还需要 Node.js 执行精确的 HarmonyOS
生产策略源码；若 `PATH` 中没有 `node`，请将 `EMSDK_NODE` 指向其可执行文件。缺少它时
配置会直接失败，而不会静默漏掉测试。HIL/QEMU 证据解析器和 Linux launchd 进程仿真
还需要 Python 3；缺少该解释器时同样会在配置阶段失败。

Use `profile-tiny`, `profile-standard`, or `profile-full` in place of
`dev-debug` to build and test that resource profile with Release optimization
and LTO. CI exercises all three profiles; exactly one is selected per build.

将 `dev-debug` 替换为 `profile-tiny`、`profile-standard` 或 `profile-full`，即可用
Release 优化与 LTO 构建并测试对应资源档位；每次构建只能选择一个档位，CI 会覆盖三档。

Build and test the installable shared-library form, including independent C11
and C++17 consumers, with the dedicated preset:

```powershell
cmake --preset ci-shared
cmake --build --preset ci-shared
ctest --preset ci-shared
```

上述预设验证可安装的动态 `mol_core`、严格公共符号边界，以及独立 C11/C++17
调用方；默认原生预设仍构建静态库。

Render the included sequence to a deterministic 24-bit mono WAV without an audio device:

```powershell
build/dev-release/apps/mol-render/mol-render examples/sequences/scale-study.molseq `
  --output scale-study.wav --format pcm24 --channels 1 --report scale-study.json
```

无需音频设备即可用上述命令将示例序列确定性地渲染为 24-bit 单声道 WAV，并生成含统计与
SHA-256 的 JSON 报告。

Inspect, validate, convert, or edit Mol Sequence v1 files with `mol-seq`:

```powershell
build/dev-release/apps/mol-seq/mol-seq inspect examples/sequences/scale-study.molseq
build/dev-release/apps/mol-seq/mol-seq binary-to-json input.molseq output.molseq.json
build/dev-release/apps/mol-seq/mol-seq midi-export input.molseq output.mid
```

List desktop outputs or play the same C4 through the system default device:

```powershell
build/dev-release/apps/mol-play/mol-play --list-devices
build/dev-release/apps/mol-play/mol-play --duration 2 --note 60
```

上述命令可列出桌面输出设备，或通过系统默认设备实时播放同一个 C4；蓝牙音箱由操作系统
作为普通音频设备提供，本项目不重复实现系统配对。

Analyze a two-channel physical stimulus/capture recording with the latency
probe. Route, device, effective buffer configuration, and tested commit are
mandatory report metadata:

```powershell
build/dev-release/tools/latency-probe/mol-latency-probe capture.wav `
  --report latency-wired.json --route built-in-wired `
  --device "interface and output model" --buffer-config "48kHz/128x3" `
  --artifact-commit (git rev-parse HEAD) --p95-limit-ms 50
```

延迟探针分析双声道物理刺激/采集 WAV，并输出原始测量值、P50、P95、最大值和采集文件
SHA-256。其合成 fixture 只验证分析器，不能替代真实设备证据。完整接线、阈值与分路由流程见
[`LATENCY_MEASUREMENT.md`](docs/testing/LATENCY_MEASUREMENT.md)。

## Physical key map / 物理键位

The default 30-key chromatic range is C4–F6 and uses physical key codes:

```text
C4–B4: Z S X D C V G B H N J M
C5–B5: Q 2 W 3 E R 5 T 6 Y 7 U
C6–F6: I 9 O 0 P [
Sustain: Space
```

## Headless and Web use / 无界面与 Web 使用

On Windows, Linux, or macOS, build or extract the portable package and start the
desktop instrument directly:

```powershell
build/dev-release/apps/mol-keyboard/mol-keyboard.exe
# From the extracted portable package:
bin/mol-keyboard.exe
```

The executable opens the packaged production UI inside a native wxWidgets
window backed by WebView2 on Windows, WebKit2GTK on Linux, and WKWebView on
macOS. Its bounded HTTP server listens only on a random `127.0.0.1` port, serves
only the local UI directory, and exits when the window closes. Standalone
synthesis uses the bundled AudioWorklet/Wasm engine and does not require the
daemon. A development UI directory can be selected with `--web-root PATH`.

Windows 用户可以直接双击构建产物或便携包中的 `mol-keyboard.exe`。它会在 wxWidgets
原生窗口中通过系统 WebView2 打开随包分发的完整界面；Linux 使用 WebKit2GTK，macOS
使用 WKWebView。本地资源服务器只监听随机的 `127.0.0.1` 端口，并随窗口关闭。默认
独立演奏使用包内 AudioWorklet/Wasm，不要求先启动后台服务。`--web-root PATH` 可用于
开发目录。

Start the foreground desktop service and control it from another terminal:

```powershell
build/dev-release/apps/mol-keyboardd/mol-keyboardd
build/dev-release/apps/molctl/molctl status
build/dev-release/apps/molctl/molctl preset set violin
build/dev-release/apps/molctl/molctl doctor
```

An additional fully native wxWidgets debugger is available for service work:

```powershell
cmake --preset dev-debug -DMOL_BUILD_NATIVE_DEBUG_GUI=ON
cmake --build --preset dev-debug --target mol-keyboard-debug
build/dev-debug/apps/mol-keyboard/mol-keyboard-debug.exe
```

It controls the independent daemon over local IPC and provides native preset,
tempo, velocity, note, sustain, recording, device, and diagnostic controls. See
[`docs/platform/DESKTOP_GUI.md`](docs/platform/DESKTOP_GUI.md) for cross-platform
dependencies, security boundaries, and simulated-display acceptance.

`mol-keyboardd` uses local-only IPC and the operating system's audio devices;
use `--null-backend` on machines without audio hardware. The same C core also
executes in the complete offline-capable Web/PWA instrument and builds into
ESP32/ESP32-S3 firmware with a configurable I2S host. From `apps/web`, run
`npm ci` followed by `npm run build`. Before `npm run test:browser`, set
`MOL_DAEMON` to a built `mol-keyboardd`; browser acceptance fails if the real
desktop service is unavailable.

![MoL Keyboard Web instrument](docs/screenshots/web-instrument.png)

The Android application packages the same local UI and renders exclusively
through JNI, Oboe/AAudio, and the C core. Build it with
`platforms/android/build-app.ps1 Debug` (or `build-app.sh` on POSIX hosts); see
[`platforms/android/README.md`](platforms/android/README.md) for the pinned SDK
and emulator test commands.

The iOS application packages that UI behind an offline-only
`WKURLSchemeHandler` and renders through RemoteIO, `AVAudioSession`, and the same
C core. On a Mac with Xcode and the pinned Emscripten SDK, run
`platforms/ios/build-app.sh Simulator` or `platforms/ios/build-app.sh Device`.
Run `platforms/ios/run-simulator-smoke.sh` to build, install, and launch the
Simulator app and fail unless the packaged production UI reaches the real
reply-capable native bridge. The device build is unsigned unless signing
variables are supplied.

`mol-keyboardd` 与 `molctl` 已可在无界面模式下通过仅限本机的 IPC 完成演奏、音色切换、
录音、回放和诊断；无音频设备的机器可使用 `--null-backend`。同一份 C 核心也已在
完整的离线 Web/PWA 乐器中通过验证，并构建为带可配置 I2S 宿主的 ESP32/ESP32-S3
固件。Android 双 ABI 应用已完成构建，并在 Android 15 模拟器上通过
UI→JNI→Oboe/AAudio→C 核心及后台/锁屏生命周期验证。iOS 完整应用源码也已实现，
通过本地 WKWebView 界面连接 AudioUnit/AVAudioSession 与同一 C 核心；其 Xcode
构建和真机验收仍须在 Apple 环境中完成；`run-simulator-smoke.sh` 会在该环境中安装并
启动模拟器 App，且仅在生产 UI 与双向原生桥接验证通过后成功退出。
HarmonyOS 共享应用源码也已用官方 OpenHarmony API 12 公共 SDK 完成 Debug/Release
兼容 HAP 构建，ArkTS 字节码及 arm64-v8a/x86_64 原生音频库均通过包内容审计；
这不替代 DevEco 下的正式 HarmonyOS 构建和设备验收。

## Platform matrix / 平台矩阵

| Target / 目标 | Implementation / 实现 | Current evidence / 当前证据 |
| --- | --- | --- |
| Windows | wxWidgets/WebView2 instrument, native debugger, daemon, CLI, WASAPI, Raw Input, WinMM MIDI | real x64 windows, 98-test GUI suite, extracted GUI/headless package, Startup lifecycle, and ARM64 cross-build passed; ARM64 and physical MIDI runtime pending / x64 实际窗口、98 项 GUI 套件、解压后 GUI/无界面包、启动服务生命周期及 ARM64 交叉构建通过，ARM64 与物理 MIDI 运行待验 |
| Linux | wxWidgets/WebKitGTK instrument, native debugger, daemon, CLI, native audio/evdev/raw-MIDI host | 100-test x86_64 suite and extracted GUI/headless package pass under WSL/Xvfb; real systemd service and AArch64 QEMU product pass; native display/audio hardware pending / WSL/Xvfb 下 100 项 x86_64 套件与解压后 GUI/无界面包通过，真实 systemd 服务及 AArch64 QEMU 产品通过，原生显示与音频硬件待验 |
| macOS | wxWidgets/WKWebView instrument, native debugger, daemon, CoreAudio, IOHIDManager, CoreMIDI | fail-closed GUI runtime and extracted-package audit lane plus app-bundle resource layout are checked in; production-source and LaunchAgent simulations pass; native Apple run pending / 已纳入失败关闭的 GUI 运行、解压包审计门禁与应用包资源布局，生产源码及 LaunchAgent 仿真通过，Apple 原生运行待验 |
| Web/PWA | Wasm AudioWorklet, offline shell | supported-browser automation passed; Safari pending / 已支持浏览器自动化通过，Safari 待验 |
| Android | Oboe/AAudio foreground service | dual-ABI builds plus Android 15 audio-focus/lifecycle simulation passed; device pending / 双 ABI 及 Android 15 音频焦点与生命周期仿真通过，真机待验 |
| iOS | AudioUnit, AVAudioSession, offline WKWebView | production background policy and hardware-key ownership simulations pass; fail-closed Simulator UI/bridge gate checked in; Apple run/device pending / 生产后台策略及硬件键所有权仿真通过，已纳入失败关闭的模拟器 UI/桥接门禁，Apple 运行与真机待验 |
| HarmonyOS | OHAudio, AVSession, continuous task | exact production policy and native bridge/host simulations plus OpenHarmony API 12 compatibility HAPs passed; formal DevEco/device pending / 精确生产策略与原生桥、OHAudio 宿主仿真及 OpenHarmony API 12 兼容 HAP 通过，正式 DevEco 与真机待验 |
| ESP32 | I2S, GPIO/BLE/Classic HID, A2DP Source | image/map and real firmware QEMU smoke passed; board HIL pending / 固件、map 与真实固件 QEMU 冒烟通过，开发板 HIL 待验 |
| ESP32-S3 | I2S, GPIO/BLE/USB HID | image/map and real firmware QEMU smoke passed; board HIL pending / 固件、map 与真实固件 QEMU 冒烟通过，开发板 HIL 待验 |

See the evidence-linked
[`PLATFORM_MATRIX.md`](docs/status/PLATFORM_MATRIX.md) for exact qualification
levels. 详细资格等级与证据链接见该文档。

Desktop MIDI is enabled by default and can be removed with
`-DMOL_ENABLE_MIDI=OFF`. `molctl devices input` reports every accessible native
endpoint as an Omni entry and as Channel 1 through Channel 16 entries; attach the
listed ID with `molctl input attach ID`. WinMM, Linux raw-MIDI, and CoreMIDI IDs
are stable only for their operating-system endpoint and should be selected from
the current device list. MIDI input covers notes and velocity, poly pressure,
CC1 modulation, continuous CC64 sustain, pitch bend, General MIDI program
mapping, reset controllers, All Notes Off, and All Sound Off.

桌面 MIDI 默认启用，也可用 `-DMOL_ENABLE_MIDI=OFF` 移除。`molctl devices input`
会把每个可访问的原生端点列为 Omni 以及 Channel 1 至 Channel 16 条目；请使用列表中的
ID 执行 `molctl input attach ID`。支持音符与力度、复音压力、CC1 调制、连续 CC64 延音、
弯音、General MIDI 音色映射、控制器复位、All Notes Off 与 All Sound Off。

## Builds and packages / 构建与打包

Activate the pinned Emscripten SDK before the Wasm commands:

```powershell
& .\.cache\emsdk\emsdk_env.ps1
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
npm.cmd --prefix apps\web ci
npm.cmd --prefix apps\web run build
```

激活仓库固定的 Emscripten SDK 后，可用上述命令构建并测试 Wasm 与生产 Web 包。
构建完成 Web 资源后，可生成含 WebView 乐器、原生服务调试器、无界面程序、C SDK、
文档、示例、许可证与 SBOM 的便携包：

```powershell
cmake --preset package-release
cmake --build --preset package-release
cpack --config build/package-release/CPackConfig.cmake -B build/packages
```

Android, iOS, HarmonyOS, and ESP-IDF exact toolchain commands are documented
under `platforms` and `docs/platform`. Portable packages are unsigned; mobile
store signing credentials are intentionally external.

Android、iOS、HarmonyOS 与 ESP-IDF 的固定工具链命令见 `platforms` 和
`docs/platform`。便携包不带签名；移动商店签名凭据必须保存在仓库之外。

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

Current limitations include unmeasured physical end-to-end latency, unverified
Apple/DevEco builds, and missing physical mobile and ESP32 long-run evidence.
The exact remaining gates are maintained in
[`KNOWN_LIMITATIONS.md`](docs/status/KNOWN_LIMITATIONS.md) and are not hidden by
generic “cross-platform” language.

当前限制包括尚未实测的物理端到端延迟、尚未完成的 Apple/DevEco 构建，以及移动与
ESP32 真机长时间证据。准确的剩余门禁记录在上述限制文档中，不以笼统的“跨平台”表述掩盖。

## License / 许可证

First-party source code is licensed under Apache-2.0. Assets and examples must
carry their own license records. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).

第一方源代码使用 Apache-2.0 许可证；资源与示例必须分别记录许可证。
