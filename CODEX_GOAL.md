# CODEX_GOAL.md — MoL Keyboard 全新项目完整实现目标

> **本文件是项目唯一、长期、可持续执行的编程目标。**  
> 当前工作目录应被视为一个全新的空白目录。不要读取、克隆、复制、迁移、参考或兼容任何过去存在的 MoL Keyboard 代码库，也不要假设旧项目的架构、接口、资源、键位或实现仍然有效。项目名称和背景故事可以保留，但全部软件必须从零设计、从零实现、从零验证。

---

## 0. Codex 执行指令

完整读取本文件后立即开始工作，不要只输出方案、分析、目录图、伪代码或待办列表。直接在当前目录创建并持续完成一个可构建、可运行、可测试、可发布的完整软件项目。

你必须遵守以下执行规则：

1. **这是从零开始的新项目。**
   - 如果当前目录没有 Git 仓库，初始化 Git 仓库。
   - 如果当前目录为空，直接建立本文件要求的工程结构。
   - 不得克隆或读取 `gitee.com/FlowerCN/MoLKeyboard`，不得导入任何同名旧项目代码、资源、提交历史或构建文件。
   - 不得为了“兼容旧实现”污染新架构。

2. **持续实现，而不是停留在规划阶段。**
   - 先检查工作区、分支、已有文件和已有完成状态。
   - 从尚未满足的最高优先级验收项开始直接实现。
   - 每完成一个最小纵向能力，立即真实构建、运行测试、记录证据并形成原子提交。
   - 完成一个阶段后继续下一个阶段，不要因已经写出框架、接口或示例就停止。
   - 除非遇到必须由用户提供的签名证书、实体硬件或封闭 SDK，不得把实现工作推回给用户。

3. **本文件是唯一主目标。**
   - 不创建第二份相互竞争的总体目标文件。
   - 可以创建架构文档、ADR、平台状态、测试报告和发布文档，但它们必须服务于本文件。
   - 每次继续工作时，先重新读取本文件，再读取 `docs/status/IMPLEMENTATION_STATUS.md` 和当前 Git 状态，避免重复工作。

4. **不得伪造完成状态。**
   - 只有真实编译通过的目标才能标记为 `build-verified`。
   - 只有真实运行并获得可审计结果的目标才能标记为 `runtime-verified`。
   - 只有在真实设备上验证过的目标才能标记为 `device-verified`。
   - 缺少 SDK、签名证书或实体设备时，继续完成其余工作，并把受阻项、所需环境、准确命令和预期结果记录清楚；不得用空壳、假测试或跳过测试来标记完成。

5. **保护已有工作。**
   - 不覆盖用户未提交修改。
   - 不删除不能确认归属的文件。
   - 修改前检查 `git status`、当前分支和最近提交。
   - 默认在 `codex/mol-keyboard-v1` 工作分支推进；如果用户已经位于明确的工作分支，则继续使用当前分支。

6. **工程结论必须由证据支撑。**
   - 不以“理论上支持”代替构建。
   - 不以 Mock 自身行为代替端到端验证。
   - 不以生成了工程目录代替完成平台适配。
   - 不以能播放一个正弦波代替完成 18 种音色和完整演奏功能。

7. **版本与外部事实必须在执行时重新核验。**
   - 使用各平台官方文档和官方 SDK。
   - 选择执行时可获得的稳定版本，并固定准确版本、提交或校验值。
   - 不根据本文件中的日期推测“最新版本”。
   - 第三方依赖必须完成许可证、来源、版本和本地修改审计。

---

## 1. 项目身份

### 1.1 正式名称

```text
英文品牌名：MoL Keyboard
中文名称：张多少的键盘
完整英文释义：More or Less Zhang's Keyboard
```

`MoL` 的大小写必须固定，不使用 `MOL Keyboard`、`Mol Keyboard` 或其他变体作为面向用户的正式名称。

### 1.2 工程命名

```text
仓库目录：mol-keyboard
CMake 项目：mol_keyboard
C 命名空间前缀：mol_
核心库：mol_core
宿主运行时：mol_runtime
无界面服务：mol-keyboardd
控制命令行：molctl
离线渲染器：mol-render
音色编译器：mol-patchc
序列转换器：mol-seq
Web 应用：mol-web
ESP-IDF 组件：mol_core
```

默认包标识集中定义，不散落硬编码：

```text
Android applicationId：cn.zhangpeixuan.molkeyboard
iOS bundle identifier：cn.zhangpeixuan.molkeyboard
HarmonyOS bundleName：cn.zhangpeixuan.molkeyboard
```

必须允许通过平台构建配置覆盖这些标识，以便正式发布前调整。

### 1.3 一句话定位

> **MoL Keyboard（张多少的键盘）是一套轻量、可嵌入、可无界面运行的数字乐器系统，将物理键盘、触摸、MIDI、GPIO 和程序事件统一转换为实时音乐演奏、录制与回放。**

英文：

> **MoL Keyboard is a lightweight, embeddable, headless-capable digital instrument system that turns keyboard, touch, MIDI, GPIO, and programmatic input into real-time musical performance, recording, and playback.**

### 1.4 项目许可证

第一方源代码默认使用：

```text
Apache-2.0
```

每个第一方源码文件使用正确的 SPDX 标识：

```text
SPDX-License-Identifier: Apache-2.0
```

音色参数、示例序列、图标、字体、音频样本和其他资源分别记录许可证。不得假定代码许可证自动覆盖资源许可证。

---

## 2. 最终产品目标

MoL Keyboard 必须同时是一套库、一个无界面运行时、一个后台服务、一个可选图形化乐器、一个 WebAssembly 应用和一个可部署到 ESP32 的嵌入式音乐引擎。

最终至少提供以下正式运行形态：

1. **嵌入式库模式**
   - 其他程序静态或动态链接 `mol_core`。
   - 调用方提供内存、事件和音频输出缓冲区。
   - 不依赖窗口、网络、文件系统、线程或操作系统。

2. **无界面进程模式**
   - `mol-keyboardd` 打开输入和音频输出设备。
   - 没有图形界面也能完成演奏、录音、回放、设备切换和状态查询。
   - 可作为用户会话后台服务运行。

3. **命令行控制模式**
   - `molctl` 控制本地服务、查询能力、选择设备、触发音符、切换音色、录制、回放、诊断和自测。

4. **离线渲染模式**
   - `mol-render` 无需音频设备，将序列确定性渲染为 WAV，并输出音频统计和哈希。

5. **Web/PWA 模式**
   - 浏览器中直接运行 C 核心编译得到的 WebAssembly。
   - 音频计算运行在 AudioWorklet，而不是主线程。
   - 可离线安装和演奏。

6. **移动应用模式**
   - Android 和 iOS 使用原生低时延音频宿主调用同一核心。
   - 图形界面关闭、退到后台或被销毁时，符合系统规则的音频会话仍可独立工作。

7. **HarmonyOS 应用模式**
   - ArkUI/ArkTS 只负责界面、生命周期和平台服务。
   - OHAudio 的 C/C++ 音频回调调用同一核心。
   - 支持合法的音频播放长时任务。

8. **ESP32 设备模式**
   - `mol_core` 作为 ESP-IDF 组件运行。
   - 支持 GPIO/USB/BLE/经典蓝牙 HID 输入中的可用组合。
   - 支持 I2S 音频输出。
   - 对支持经典蓝牙的 ESP32 芯片提供 A2DP Source 输出到蓝牙音箱。

---

## 3. 不可妥协的设计原则

### 3.1 核心不依赖平台

`mol_core` 必须使用 **ISO C11** 实现，并保持 freestanding-friendly。核心中禁止出现：

- Flutter、Dart、React、Vue、Electron、Qt、JUCE；
- SDL、DOM、WebAudio、WASAPI、Core Audio、Oboe、OHAudio；
- Win32、POSIX、Android、Apple、HarmonyOS、ESP-IDF 类型；
- 文件路径、线程句柄、窗口句柄、Socket 或平台日志；
- C++ STL、异常、RTTI 或 C++ ABI；
- 隐式全局单例和可变全局状态。

核心只接受：

- 初始化配置；
- 调用方提供的内存；
- 版本化音乐命令；
- 预编译音色数据；
- 输出音频缓冲区；
- 状态查询和事件读取。

### 3.2 UI 永远是可选项

- 删除所有 UI 后，核心、服务、CLI、离线渲染、WebAssembly 音频模块和 ESP32 固件仍然能够构建和运行。
- UI 不拥有音乐状态的唯一真相。
- 和弦、琶音、延音、节拍器、录音、音色和声部逻辑禁止写在 UI 中。
- UI 只能发送命令、读取只读状态快照和展示实际发声事件。

### 3.3 音频线程必须实时安全

音频回调和 `mol_engine_render_*()` 调用链中禁止：

- 动态内存分配和释放；
- 文件、网络、数据库和持久化访问；
- 锁、条件变量、睡眠和阻塞等待；
- 日志格式化和终端输出；
- 调用 JavaScript、Dart、Java、Kotlin、Swift 或 ArkTS；
- 抛出异常；
- 加载、解析或编译音色；
- 无界循环和无界容器增长。

### 3.4 调用方提供内存

核心不得强制依赖 `malloc()`。使用如下模式：

```c
size_t mol_engine_query_memory(const mol_engine_config_t* config);

mol_result_t mol_engine_init(
    void* memory,
    size_t memory_size,
    const mol_engine_config_t* config,
    mol_engine_t** out_engine);
```

要求：

- 内存大小和对齐可查询；
- 初始化失败不得部分泄漏状态；
- `mol_engine_shutdown()` 不释放调用方内存；
- 初始化完成后，渲染路径不再扩容；
- 所有容量由配置显式限定；
- Tiny/Standard/Full 配置使用同一 API。

### 3.5 同一音乐语义，多种资源档位

所有平台共享：

- 相同音符和手势语义；
- 相同音阶、和弦、琶音和延音规则；
- 相同录音事件格式；
- 相同音色 ID 和参数 Schema；
- 相同状态机和错误语义。

不同性能档位可以使用不同：

- 最大复音数；
- 重采样质量；
- 效果器长度；
- 过采样倍数；
- 可选采样层；
- 输出声道数。

不得声称 ESP32 与桌面设备在听感、效果长度或蓝牙延迟上完全相同，但必须保证音乐行为一致。

---

## 4. 明确的平台现实边界

以下边界必须在代码能力矩阵、README 和运行时诊断中明确表达，不得通过虚假抽象掩盖：

1. 桌面和移动平台上的蓝牙音箱通常由操作系统完成配对和音频路由；应用选择或跟随系统音频输出，不自行实现通用蓝牙配对协议。
2. 蓝牙音频自身存在明显缓冲延迟，不能与有线、内置扬声器或低时延 USB 音频使用同一延迟门槛。
3. Android、iOS 和 HarmonyOS 的后台运行必须遵守各自前台服务、音频会话和长时任务规则。
4. 通用蓝牙键盘在移动系统中通常由系统作为硬件键盘管理；应用退到后台后不保证继续收到普通按键事件。
5. Web 页面只有在获得焦点时才能可靠接收普通键盘输入，音频启动还必须满足浏览器用户手势策略。
6. 原始 ESP32 支持经典蓝牙，可实现 A2DP Source；ESP32-S3 仅支持 BLE，不能把经典蓝牙 A2DP Source 作为其验收项。
7. 平台不支持某项能力时，必须通过运行时 capability 明确返回 `unsupported`，不能静默降级成无效按钮。

---

## 5. 目标平台和支持等级

### 5.1 平台矩阵

| 平台 | 核心库 | 无界面 | 实时音频 | UI | 后台音频 | 物理键盘 | 嵌入式输出 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Windows x64/arm64 | 必须 | 必须 | 必须 | Web 控制 UI | 必须 | 必须 | 不适用 |
| Linux x86_64/aarch64 | 必须 | 必须 | 必须 | Web 控制 UI | 必须 | 必须 | 不适用 |
| macOS arm64/x64 | 必须 | 必须 | 必须 | Web 控制 UI | 必须 | 必须 | 不适用 |
| WebAssembly | 必须 | Worker/Worklet | 必须 | 必须 | 浏览器规则内 | 页面聚焦时 | 不适用 |
| Android arm64 | 必须 | Service | 必须 | 必须 | 必须 | 前台可用 | 不适用 |
| iOS arm64 | 必须 | Audio runtime | 必须 | 必须 | 必须 | 前台可用 | 不适用 |
| HarmonyOS arm64 | 必须 | Continuous task | 必须 | 必须 | 必须 | 平台允许时 | 不适用 |
| ESP32 | 必须 | 必须 | 必须 | 可选 Web UI | 常驻固件 | HID/GPIO | I2S + A2DP |
| ESP32-S3 | 必须 | 必须 | 必须 | 可选 Web UI | 常驻固件 | BLE/USB HID/GPIO | I2S |

### 5.2 支持状态定义

```text
planned          仅有目标，尚无工程
scaffolded       存在工程入口，但未真实构建
build-verified   在对应工具链中真实构建成功
runtime-verified 在模拟器或主机上真实运行
 device-verified 在真实设备上通过验收
release-ready    构建、运行、文档、测试、许可和打包全部满足
```

`docs/status/PLATFORM_MATRIX.md` 必须列出每个平台的当前状态、验证日期、工具链版本、设备型号、命令和证据摘要。

---

## 6. 总体架构

```text
Input Sources
  ├─ HID keyboard
  ├─ Touch / pointer
  ├─ MIDI 1.0
  ├─ GPIO key matrix
  ├─ BLE / USB HID
  ├─ CLI / RPC
  └─ Sequence playback
          │
          ▼
Platform Input Adapters
          │ normalized gesture events
          ▼
MoL Runtime
  ├─ input routing
  ├─ command queue
  ├─ device lifecycle
  ├─ persistence
  ├─ service / IPC
  └─ state publication
          │ versioned C commands
          ▼
MoL Core — ISO C11
  ├─ mapping / transpose
  ├─ scale lock
  ├─ chord expansion
  ├─ transport / metronome
  ├─ arpeggiator
  ├─ sustain / portamento
  ├─ voice allocation
  ├─ procedural instruments
  ├─ effects / mixer / limiter
  ├─ event recording / playback
  └─ deterministic state
          │ float PCM
          ▼
Platform Audio Hosts
  ├─ miniaudio desktop host
  ├─ Core Audio / AudioUnit
  ├─ Oboe / AAudio
  ├─ OHAudio
  ├─ AudioWorklet / WebAssembly
  ├─ ESP-IDF I2S
  ├─ ESP-IDF A2DP Source
  └─ WAV offline sink
```

核心不得主动创建线程。宿主决定线程模型并调用核心。

---

## 7. 推荐仓库结构

在不牺牲可构建性的前提下，逐步建立并保持以下结构：

```text
mol-keyboard/
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
├── NOTICE
├── README.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── SECURITY.md
├── CODE_OF_CONDUCT.md
├── CODEX_GOAL.md
├── .clang-format
├── .clang-tidy
├── .editorconfig
├── .gitignore
│
├── cmake/
│   ├── MolOptions.cmake
│   ├── MolWarnings.cmake
│   ├── MolSanitizers.cmake
│   ├── MolDependencies.cmake
│   └── toolchains/
│
├── include/mol/
│   ├── mol.h
│   ├── version.h
│   ├── result.h
│   ├── engine.h
│   ├── command.h
│   ├── event.h
│   ├── patch.h
│   ├── sequence.h
│   └── capabilities.h
│
├── src/
│   ├── base/
│   ├── memory/
│   ├── music/
│   ├── transport/
│   ├── performance/
│   ├── synth/
│   ├── dsp/
│   ├── effects/
│   ├── recording/
│   ├── patch/
│   ├── sequence/
│   ├── engine/
│   └── c_api/
│
├── runtime/
│   ├── include/mol_runtime/
│   ├── src/
│   │   ├── command_queue/
│   │   ├── state_snapshot/
│   │   ├── device_manager/
│   │   ├── input_router/
│   │   ├── persistence/
│   │   ├── service/
│   │   └── diagnostics/
│   └── tests/
│
├── platforms/
│   ├── null/
│   ├── wav/
│   ├── desktop/
│   │   ├── audio/
│   │   ├── windows/
│   │   ├── linux/
│   │   └── macos/
│   ├── web/
│   │   ├── emscripten/
│   │   ├── audio_worklet/
│   │   └── js_bridge/
│   ├── android/
│   │   ├── app/
│   │   ├── jni/
│   │   └── audio/
│   ├── ios/
│   │   ├── app/
│   │   ├── bridge/
│   │   └── audio/
│   ├── harmony/
│   │   ├── entry/
│   │   ├── napi/
│   │   └── audio/
│   └── esp32/
│       ├── components/mol_core/
│       ├── main/
│       ├── boards/
│       └── sdkconfig/
│
├── apps/
│   ├── mol-keyboardd/
│   ├── molctl/
│   ├── mol-render/
│   └── web/
│       ├── src/
│       ├── public/
│       └── tests/
│
├── protocols/
│   ├── jsonrpc/
│   ├── wire/
│   └── schemas/
│
├── presets/
│   ├── source/
│   ├── generated/
│   ├── schemas/
│   └── licenses/
│
├── sequences/
│   └── examples/
│
├── tools/
│   ├── mol-patchc/
│   ├── mol-seq/
│   ├── audio-analyze/
│   ├── latency-probe/
│   ├── license-audit/
│   └── sbom/
│
├── tests/
│   ├── unit/
│   ├── property/
│   ├── fuzz/
│   ├── conformance/
│   ├── golden_audio/
│   ├── integration/
│   ├── native_wasm/
│   ├── platform/
│   └── hardware/
│
├── docs/
│   ├── architecture/
│   ├── adr/
│   ├── api/
│   ├── audio/
│   ├── formats/
│   ├── platform/
│   ├── product/
│   ├── testing/
│   ├── releases/
│   └── status/
│       ├── IMPLEMENTATION_STATUS.md
│       ├── PLATFORM_MATRIX.md
│       ├── QUALITY_GATES.md
│       └── KNOWN_LIMITATIONS.md
│
├── third_party/
│   ├── manifest.lock.json
│   ├── LICENSES/
│   └── patches/
│
└── packaging/
    ├── windows/
    ├── linux/
    ├── macos/
    ├── web/
    ├── android/
    ├── ios/
    └── harmony/
```

不要为了匹配目录图一次性创建大量空目录。目录必须随可运行纵向功能逐步出现。禁止保留空壳模块和无引用文件。

---

## 8. 构建系统和构建档位

### 8.1 原生构建

使用：

```text
CMake + CMakePresets + Ninja
```

至少提供：

```text
configure presets:
  dev-debug
  dev-release
  ci-linux-gcc
  ci-linux-clang
  ci-windows-msvc
  ci-macos
  wasm-debug
  wasm-release
  android-arm64
  ios-simulator
  ios-device

build profiles:
  tiny
  standard
  full
```

### 8.2 构建档位

#### Tiny

面向 MCU、ESP32 和最小 Wasm：

- 纯过程音色；
- 至少 8 个稳定复音声部，目标 12；
- 单声道或立体声可配置；
- 32 kHz、44.1 kHz、48 kHz；
- 缩短版 Chorus、Delay、Reverb Lite；
- 禁止采样器和大资源；
- 固定容量；
- 无文件系统也可运行；
- 18 个内置音色 ID 全部可用。

#### Standard

默认桌面、移动和 Web：

- 至少 32 声部；
- 48 kHz 立体声默认；
- 完整 Chorus、Delay、Reverb；
- 完整录音和回放；
- 本地服务、Web UI 和设备管理；
- 可选小型采样层，但不作为 18 个基础音色的依赖。

#### Full

高性能设备：

- 至少 64 声部；
- 高质量重采样和过采样；
- 更长效果器缓冲区；
- 可选采样音色包；
- 离线高质量渲染。

三个档位必须使用同一公共 API、同一事件语义和同一音色 Schema。

### 8.3 Feature 选项

至少提供明确且可组合的 CMake 选项：

```text
MOL_BUILD_TESTS
MOL_BUILD_TOOLS
MOL_BUILD_DAEMON
MOL_BUILD_WEB_SERVER
MOL_ENABLE_MIDI
MOL_ENABLE_SAMPLER
MOL_ENABLE_CHORUS
MOL_ENABLE_DELAY
MOL_ENABLE_REVERB
MOL_ENABLE_ASSERTIONS
MOL_ENABLE_SANITIZERS
MOL_ENABLE_COVERAGE
MOL_ENABLE_LTO
MOL_PROFILE_TINY
MOL_PROFILE_STANDARD
MOL_PROFILE_FULL
```

无效组合必须在 configure 阶段明确报错。

---

## 9. 公共 C API 与 ABI

### 9.1 ABI 规则

- 所有公共接口为 C ABI。
- 所有结构体包含 `struct_size` 和适用时的 `api_version`。
- 不跨 ABI 传递 C++ 对象、STL、异常、变长数组或平台句柄。
- 使用固定宽度整数类型。
- 枚举的底层传输使用明确整数。
- 字符串使用 UTF-8，所有缓冲区显式给出容量。
- 所有返回值使用 `mol_result_t`，错误可转为稳定错误字符串。
- ABI 向后兼容策略记录在 `docs/api/ABI_POLICY.md`。
- 生成 ABI dump，并在 CI 中检查破坏性变更。

### 9.2 必须实现的核心接口

至少包括：

```c
typedef struct mol_engine mol_engine_t;
typedef uint64_t mol_frame_index_t;
typedef uint64_t mol_gesture_id_t;

uint32_t mol_get_api_version(void);
const char* mol_get_version_string(void);

size_t mol_engine_query_memory(const mol_engine_config_t* config);

mol_result_t mol_engine_init(
    void* memory,
    size_t memory_size,
    const mol_engine_config_t* config,
    mol_engine_t** out_engine);

void mol_engine_shutdown(mol_engine_t* engine);
void mol_engine_reset(mol_engine_t* engine);

mol_result_t mol_engine_submit(
    mol_engine_t* engine,
    const mol_command_t* command);

mol_result_t mol_engine_render_interleaved_f32(
    mol_engine_t* engine,
    float* output,
    uint32_t frame_count,
    uint32_t channel_count);

mol_result_t mol_engine_render_planar_f32(
    mol_engine_t* engine,
    float* const* output_channels,
    uint32_t frame_count,
    uint32_t channel_count);

uint32_t mol_engine_poll_events(
    mol_engine_t* engine,
    mol_event_t* events,
    uint32_t capacity);

mol_result_t mol_engine_get_state(
    const mol_engine_t* engine,
    mol_engine_state_t* state);

mol_capability_flags_t mol_engine_get_capabilities(
    const mol_engine_t* engine);
```

### 9.3 命令类型

至少支持：

```text
NOTE_ON
NOTE_OFF
POLY_PRESSURE
PITCH_BEND
SUSTAIN
ALL_NOTES_OFF
ALL_SOUND_OFF
SET_MASTER_GAIN
SET_PRESET
SET_PARAMETER
SET_OCTAVE_SHIFT
SET_TRANSPOSE
SET_SCALE
SET_CHORD_MODE
SET_ARPEGGIATOR
SET_TEMPO
SET_TIME_SIGNATURE
TRANSPORT_START
TRANSPORT_STOP
TRANSPORT_SEEK
RECORD_START
RECORD_STOP
PLAYBACK_START
PLAYBACK_STOP
LOAD_SEQUENCE
RESET_ENGINE
```

每个命令携带：

```text
struct_size
api_version
command_type
target_frame
source_id
gesture_id
payload
```

`target_frame` 使用音频帧时间。`MOL_FRAME_IMMEDIATE` 表示安全地安排到下一个可处理帧，而不是在调用线程直接修改声部。

### 9.4 事件类型

至少提供：

```text
NOTE_STARTED
NOTE_RELEASED
NOTE_ENDED
GESTURE_MAPPED
PRESET_CHANGED
TRANSPORT_CHANGED
RECORDING_CHANGED
VOICE_STOLEN
XRUN_REPORTED
COMMAND_DROPPED
DEVICE_STATE_CHANGED
ERROR_REPORTED
```

UI 必须根据实际事件显示发声状态，不能根据按键动画自行推测。

---

## 10. 内存、线程与实时模型

### 10.1 单实例与多实例

- 不使用全局单例。
- 同一进程可创建多个独立引擎。
- 每个引擎拥有独立内存、随机种子、传输状态、声部和录音状态。
- 多实例测试必须覆盖并行运行和销毁顺序。

### 10.2 命令队列

宿主运行时使用有界 SPSC 环形队列连接控制线程和音频线程：

```text
Input/UI/RPC thread
        ↓
Bounded command queue
        ↓
Audio callback thread
        ↓
Mol Core render
```

要求：

- 队列容量可配置；
- 队列满时返回明确错误和统计计数；
- 不阻塞音频线程；
- 使用经过验证的内存序；
- 提供无原子环境下的单线程直接提交模式；
- 队列实现有压力测试、TSAN 测试和 wrap-around 测试。

### 10.3 状态快照

控制线程不得读取音频线程正在修改的内部对象。使用：

- 双缓冲或版本化只读快照；
- 有界事件队列；
- 原子发布的轻量统计；
- 不暴露裸内部指针。

### 10.4 音频时间

统一使用：

```c
typedef uint64_t mol_frame_index_t;
```

要求：

- 所有节拍、琶音、录音和回放最终转换为音频帧；
- 支持一个音频块内部的 sample-accurate 事件；
- 事件落在块中间时拆分渲染区间，而不是量化到块边界；
- UI 时间、墙钟时间和系统 monotonic 时间只在宿主边界转换；
- 序列回放不依赖 UI 帧率和定时器精度。

### 10.5 数值安全

- 内部默认使用 `float` PCM。
- 输入和输出必须检查有限值。
- 避免 NaN、Infinity 和 denormal 扩散。
- 所有滤波器在参数极值和采样率变化下保持稳定。
- 音频输出经过最终限幅。
- 引擎重置、切换音色和停止时使用短斜坡避免爆音。

---

## 11. 音乐处理顺序与确定语义

处理顺序固定为：

```text
Physical / Programmatic Input
        ↓
Input Mapping
        ↓
Octave Shift and Semitone Transpose
        ↓
Scale Lock
        ↓
Chord Expansion
        ↓
Arpeggiator and Transport Scheduling
        ↓
Sustain and Monophonic Portamento
        ↓
Voice Allocation
        ↓
Instrument Synthesis
        ↓
Chorus / Delay / Reverb Sends
        ↓
Master Mixer
        ↓
Limiter and Output Ramp
```

任何平台和 UI 不得改变顺序。

### 11.1 手势 ID

每次物理按键、触摸、MIDI Note On、GPIO 按下或 RPC 触发都生成唯一 `gesture_id`。

一个输入手势经过音阶、和弦和琶音后可能生成多个实际音符。Note Off 必须根据原始 `gesture_id` 释放对应音符集合，不能根据当前音阶或当前和弦重新计算。

必须测试以下场景：

- 按住音符期间切换八度；
- 按住音符期间切换音阶；
- 按住音符期间切换和弦；
- 延音开启时释放原始按键；
- 琶音进行中关闭模式；
- 页面失焦、设备断连和应用切后台；
- 同音高由多个独立手势同时触发。

不得出现卡音或误释放其他手势。

### 11.2 键盘音域

默认 30 个连续半音：

```text
C4–F6
```

默认物理键位使用 `KeyboardEvent.code` 或 USB HID Usage，而不是本地化字符：

```text
C4–B4:
KeyZ KeyS KeyX KeyD KeyC KeyV KeyG KeyB KeyH KeyN KeyJ KeyM

C5–B5:
KeyQ Digit2 KeyW Digit3 KeyE KeyR Digit5 KeyT Digit6 KeyY Digit7 KeyU

C6–F6:
KeyI Digit9 KeyO Digit0 KeyP BracketLeft
```

要求：

- 忽略操作系统自动 key repeat 产生的重复 Note On；
- 正确处理 key up；
- `Space` 默认作为延音踏板；
- 八度快捷键不得与 30 个音符键冲突；
- 支持自定义映射文件；
- 输入法和键盘语言变化不得改变物理键位映射；
- 窗口失焦、页面隐藏、设备拔出时执行 `ALL_NOTES_OFF`。

### 11.3 八度和移调

- 八度范围：`-3` 至 `+3`；
- 半音移调范围：`-24` 至 `+24`；
- 超出 MIDI 0–127 的音符不发声并产生可诊断事件；
- 改变八度和移调只影响之后的新手势，不改变已发声音符。

### 11.4 音阶保护

至少实现：

```text
Chromatic
Major
Natural Minor
Major Pentatonic
Minor Pentatonic
Blues
Dorian
Mixolydian
```

要求：

- 主音 C–B 可选；
- 明确定义向下、向上或最近音映射策略；
- 默认使用最近音，等距时采用确定规则；
- 提供独立单元测试覆盖全部 12 个主音和边界音高。

### 11.5 和弦模式

至少实现：

```text
Off
Major
Minor
Sus2
Sus4
Dominant 7
Major 7
Minor 7
Power 5
Octave
```

要求：

- 和弦音程使用数据表；
- 超出范围的和弦音安全忽略；
- 每个和弦音保持与原手势的归属关系；
- 和弦模式可与音阶保护和琶音器组合；
- 声部不足时使用确定性的抢占规则。

### 11.6 琶音器

至少支持：

```text
Up
Down
UpDown
DownUp
AsPlayed
RandomDeterministic
```

节奏分辨率至少支持：

```text
1/4
1/8
1/8T
1/16
1/16T
1/32
```

要求：

- 使用统一 Transport；
- Random 模式使用可设置种子并可确定性回放；
- 支持 gate 比例；
- 支持 1–4 个八度扩展；
- 停止、切换模式和失焦时不遗留音符；
- 运行 30 分钟不产生累计节拍漂移。

### 11.7 延音踏板

- MIDI CC64 语义；
- 按住踏板时，已经收到 Note Off 的音符进入 held-by-pedal 状态；
- 踏板释放时，仅释放不再被任何手势按住的音符；
- 支持半踏板值作为连续参数，Tiny 档位可量化为开关但必须报告 capability；
- `ALL_SOUND_OFF` 立即静音，`ALL_NOTES_OFF` 按正常 release 处理。

### 11.8 滑音

- 仅在单音模式或明确支持的音色中启用；
- 和弦模式默认禁用；
- 支持 legato-only 和 always 两种触发方式；
- 时间范围 0–2000 ms；
- 使用指数或线性音高轨迹，行为写入文档并测试。

### 11.9 节拍器与 Transport

- BPM 范围：30–300；
- 默认：100 BPM、4/4；
- 支持 2/4、3/4、4/4、5/4、6/8；
- 小节首拍使用不同重音；
- 节拍器音色由核心生成，不依赖外部媒体文件；
- Transport 为琶音、节拍器、录音和回放的唯一时间源。

---

## 12. 声部管理和 DSP 引擎

### 12.1 声部生命周期

声部至少具有：

```text
Idle
Attack
Decay
Sustain
Release
HeldByPedal
StolenRamp
```

抢占优先级固定：

1. 已进入 Release 且能量最低；
2. HeldByPedal 中能量最低；
3. 最老且能量最低的活动声部；
4. 使用短交叉渐变，禁止硬切产生爆音。

每次抢占产生 `VOICE_STOLEN` 事件和统计。

### 12.2 基础 DSP 模块

必须自行实现并测试以下轻量模块：

- phase accumulator；
- sine lookup/interpolation；
- polyBLEP saw、square 和 pulse；
- triangle；
- white/pink-ish noise source；
- ADSR 和多段包络；
- one-pole smoothing；
- state-variable filter；
- biquad filter；
- LFO；
- 2-operator FM；
- additive partial bank；
- Karplus–Strong/plucked-string；
- modal/resonator bank；
- soft saturation；
- DC blocker；
- master limiter；
- dB/linear 转换和参数平滑。

不得将 18 个音色实现为 18 个仅修改波形名称的正弦波示例。

### 12.3 效果器

#### Chorus

- 调制延迟线；
- rate、depth、mix；
- 参数平滑；
- Tiny 档位缩短缓冲区。

#### Delay

- delay time、feedback、mix；
- 支持毫秒和节拍同步；
- feedback 有稳定上限；
- 参数变化无爆音；
- 支持清空尾音。

#### Reverb

- 使用轻量 Schroeder/Moorer 或同等可移植算法；
- pre-delay、size、damping、mix；
- Tiny 使用缩短 comb/all-pass 配置；
- 不允许无限反馈和 NaN；
- impulse response 有自动测试。

### 12.4 主混音

```text
Voice Mix
  → Instrument Gain
  → Effect Sends
  → Dry/Wet Sum
  → Master Gain
  → DC Blocker
  → Limiter
  → Output Ramp
```

默认值：

```text
master gain：约 -12 dBFS
limiter ceiling：约 -1 dBFS
```

“儿童模式音量限制”只能限制数字增益，文档不得声称能够保证实际声压级安全，因为扬声器、耳机和系统音量不受引擎完全控制。

---

## 13. 18 种内置音色

必须提供以下稳定 ID、中文名和英文名：

| ID | 中文 | English |
|---|---|---|
| `grand-piano` | 大钢琴 | Grand Piano |
| `electric-piano` | 电钢琴 | Electric Piano |
| `harpsichord` | 羽管键琴 | Harpsichord |
| `church-organ` | 教堂风琴 | Church Organ |
| `jazz-organ` | 爵士风琴 | Jazz Organ |
| `nylon-guitar` | 尼龙弦吉他 | Nylon Guitar |
| `steel-guitar` | 钢弦吉他 | Steel Guitar |
| `violin` | 小提琴 | Violin |
| `cello` | 大提琴 | Cello |
| `flute` | 长笛 | Flute |
| `clarinet` | 单簧管 | Clarinet |
| `synth-lead` | 合成主音 | Synth Lead |
| `synth-pad` | 合成铺底 | Synth Pad |
| `synth-bass` | 合成贝斯 | Synth Bass |
| `choir` | 合唱 | Choir |
| `vibraphone` | 颤音琴 | Vibraphone |
| `harp` | 竖琴 | Harp |
| `music-box` | 音乐盒 | Music Box |

### 13.1 音色实现要求

- 基础 18 音色全部由可移植过程合成实现，不依赖大型 SoundFont 或未经授权样本。
- 音色可以组合 subtractive、FM、additive、pluck、modal、noise 和 formant 模块。
- 每个音色必须拥有独立的音量、包络、滤波、动态和效果发送配置。
- 每个音色至少在 C3–C7 的有效区域进行自动测试。
- 音色切换不造成卡音、NaN 或明显爆音。
- 旧音色可自然释放，新 Note On 使用新音色；也提供显式 hard-switch 配置。
- 18 个音色的感知响度进行基础校准，避免切换时出现极端音量差。
- README 明确说明它们是“内置乐器音色预设”，不虚假声称为高端采样钢琴。

### 13.2 数据驱动 Patch

音色源文件使用可审计 JSON，构建时由 `mol-patchc` 编译为固定、版本化二进制：

```text
source .molpatch.json
      ↓ schema validation
mol-patchc
      ↓ deterministic compile
compiled .molpatch
      ↓ optional C byte array generation
embedded built-in preset
```

二进制至少包含：

```text
magic
format version
header size
payload size
feature flags
preset id hash
CRC32
fixed-layout parameter payload
```

要求：

- 运行时不在音频线程解析 JSON；
- ESP32 可把编译后音色直接作为 `const` 数据放入 flash；
- 所有整数端序显式定义；
- 非法、截断、超大和未知版本输入必须安全拒绝；
- parser 进入 fuzz 测试；
- `mol-patchc` 同一输入生成字节完全一致的输出。

### 13.3 可选采样器

Standard/Full 可以实现可选采样器，但不得成为 v1 基础音色的硬依赖：

- PCM WAV 导入；
- 可选 IMA ADPCM；
- root note、key range、velocity range、loop；
- 离线重采样；
- 资源哈希和许可证清单；
- Tiny 构建关闭采样器后仍通过全部核心音乐行为测试。

---

## 14. 输入系统

### 14.1 统一输入事件

平台输入先转换为：

```text
source_id
device_id
gesture_id
input_type
physical_code
note_or_control
value
timestamp
flags
```

核心不感知“Windows 键盘”或“ESP32 GPIO”。

### 14.2 必须实现的输入适配器

1. **Script/Test Input**：确定性测试和离线渲染。
2. **Desktop HID Keyboard**：
   - Windows Raw Input 或等效用户会话 API；
   - Linux evdev/udev；
   - macOS IOHIDManager，明确权限要求。
3. **Web Keyboard/Pointer**：`KeyboardEvent.code`、Pointer Events、多点触摸。
4. **MIDI 1.0**：Note、CC64、pitch bend、program change 的合理映射。
5. **Android/iOS/Harmony Hardware Keyboard**：应用前台时接收系统键盘事件。
6. **ESP32 HID Host**：芯片能力允许时支持 BLE、经典蓝牙或 USB HID。
7. **ESP32 GPIO Matrix**：可配置行列、去抖、ghosting 策略和长按。
8. **RPC/Wire Input**：供 `molctl`、Web 控制器和自动化使用。

### 14.3 热插拔和异常

- 输入设备断开时释放该 `source_id` 拥有的全部手势；
- 设备重连不复用仍在活动的旧手势 ID；
- 重复连接不创建重复输入；
- 权限不足返回可操作错误；
- `molctl doctor` 能说明输入不可用的原因。

### 14.4 MIDI

至少支持：

```text
Note On / Off
Velocity
CC64 Sustain
CC1 Modulation
Pitch Bend
Channel filtering
All Notes Off
All Sound Off
```

MIDI 输入不是核心必需依赖，使用适配器接入。没有 MIDI SDK 的平台仍可构建核心。

---

## 15. 音频输出系统

### 15.1 统一音频宿主接口

宿主负责：

- 枚举输出设备；
- 请求采样率、格式、声道和低时延模式；
- 创建音频回调；
- 把实际采样率、block size 和 latency 信息传给核心；
- 处理设备断开、路由变化、焦点、中断和重启；
- 将核心 float PCM 转换为平台格式；
- 记录 underrun/xrun。

核心负责生成 PCM，不直接操作设备。

### 15.2 桌面

优先使用固定版本的 miniaudio 作为桌面设备抽象，保留后端能力查询：

```text
Windows：WASAPI
Linux：PipeWire/PulseAudio/ALSA/JACK 中实际可用者
macOS：Core Audio
```

要求：

- 枚举设备并保存稳定标识；
- 默认跟随系统输出；
- 支持选择蓝牙音箱已暴露的系统音频设备；
- 设备消失后安全切回默认或静音并报告；
- 不在应用内重新实现通用蓝牙配对。

### 15.3 Web

必须使用：

```text
Emscripten-compiled mol_core.wasm
        +
AudioWorkletProcessor
```

要求：

- 音频 DSP 不运行在浏览器主线程；
- AudioWorklet 中预分配内存；
- 基础版本不依赖 SharedArrayBuffer；
- 基础命令通过 MessagePort 批量传递；
- 在 cross-origin isolated 环境提供 SharedArrayBuffer/SPSC 快速路径；
- 页面必须由用户手势启动 AudioContext；
- 处理 suspend/resume、visibility change 和设备变化；
- AudioWorklet 的 128-frame quantum 不应破坏 sample-accurate 调度；
- Wasm 内存增长后不得长期缓存失效的 TypedArray；
- 生产部署使用 HTTPS；
- 提供离线 PWA Service Worker；
- 不使用已废弃 ScriptProcessorNode。

### 15.4 Android

使用：

```text
Kotlin shell / foreground service
        ↓ JNI
C/C++ runtime
        ↓
Oboe
        ↓
mol_core
```

要求：

- 优先 AAudio，使用 Oboe 兼容回退；
- 请求低时延 performance mode；
- 使用设备实际采样率和合理 burst；
- 音频回调不进入 JVM；
- 正确管理 Audio Focus；
- 后台运行使用合法 `mediaPlayback` 前台服务和持续通知；
- 后台播放必须由用户明确启动；
- 响应耳机/蓝牙路由变化和中断；
- 蓝牙键盘普通按键只承诺应用前台可用；
- 至少支持 arm64-v8a，并为测试支持 x86_64 模拟器；
- 不使用已废弃 OpenSL ES 作为直接新实现。

### 15.5 iOS/macOS Apple 原生宿主

使用：

```text
Swift / Objective-C++ lifecycle
        ↓
C ABI
        ↓
AudioUnit / Core Audio render callback
        ↓
mol_core
```

iOS 要求：

- 配置 AVAudioSession；
- 支持 playback 场景、静音键策略和背景音频 capability；
- 处理 interruption、route change、media services reset；
- 仅在用户启动且实际需要音频时维持背景播放；
- 进入后台但没有活动音频时释放资源；
- 蓝牙、AirPlay 和有线输出遵循系统路由；
- 硬件键盘事件按 UIKit 能力在前台处理；
- 不声称可以作为无限制系统守护进程运行。

### 15.6 HarmonyOS

使用：

```text
ArkUI / ArkTS
        ↓ Node-API
C/C++ platform host
        ↓
OHAudio callback
        ↓
mol_core
```

要求：

- 申请并检查低时延模式，失败时回退普通模式；
- 查询和记录实际 fast status；
- 使用合适 StreamUsage；
- 处理 AudioSession 停用和音频焦点；
- 后台播放使用官方音频播放长时任务；
- 如需持续蓝牙交互，使用合法的 Bluetooth Interaction 长时任务模式；
- ArkTS 不生成 PCM；
- Node-API 层只传递配置、命令和状态；
- 只有真实 DevEco/HarmonyOS SDK 构建通过后才能标记 build-verified；
- 只有真实设备播放通过后才能标记 device-verified。

### 15.7 ESP32 I2S

- 使用 ESP-IDF 正式 I2S API；
- DMA 缓冲区固定并预分配；
- 音频任务优先级和 core affinity 明确；
- float PCM 转 int16 使用饱和转换和可选 dither；
- 支持外部 I2S DAC/codec 板级配置；
- 板级 GPIO 不硬编码在引擎；
- 提供至少一个参考板配置和接线文档；
- 看门狗、欠载和设备重启有可诊断统计。

### 15.8 ESP32 A2DP Source

仅在 SoC 和 ESP-IDF capability 支持经典蓝牙时启用：

- 将 MoL PCM 作为 A2DP Source 发送至蓝牙音箱；
- 处理扫描、配对、连接、断开、重连和 AVRCP 基础状态；
- A2DP 编码和缓冲不得阻塞实时渲染；
- 连接失败回退 I2S 或静音；
- 蓝牙高延迟单独测量，不纳入有线低时延硬门槛；
- ESP32-S3 构建必须明确关闭此 capability，并继续通过 I2S 验收。

### 15.9 离线 WAV 输出

`mol-render` 必须支持：

```text
16-bit PCM WAV
24-bit PCM WAV
32-bit float WAV
mono / stereo
custom sample rate
normal render
high-quality offline render
```

输出包含：

- duration；
- peak；
- RMS；
- clipped sample count；
- NaN/Inf count；
- underrun count（离线应为 0）；
- SHA-256；
- JSON report。

---

## 16. 无界面服务、CLI 和控制协议

### 16.1 `mol-keyboardd`

服务必须：

- 默认以前台模式启动，`--daemon` 或平台服务配置为可选；
- 作为普通用户运行，不默认要求 root/管理员；
- 打开选定输入和输出；
- 初始化核心；
- 提供本地 IPC；
- 管理配置、序列和状态；
- 可完全不加载 Web UI；
- 在没有音频设备时可使用 null 或 WAV sink 运行；
- 收到 SIGTERM/系统停止时安全释放全部音符和设备。

### 16.2 本地 IPC

控制平面使用 JSON-RPC 2.0：

```text
POSIX：Unix domain socket
Windows：Named Pipe
可选浏览器控制：loopback WebSocket
```

默认：

- 只允许本机；
- 不监听局域网；
- WebSocket 生成随机会话 token；
- 检查 Origin；
- 不提供任意文件读写或命令执行；
- 所有路径经过允许目录和规范化检查。

### 16.3 必须实现的 RPC 方法

```text
system.getInfo
system.getCapabilities
system.getMetrics
system.shutdown
engine.getState
engine.reset
engine.allNotesOff
engine.allSoundOff
preset.list
preset.select
preset.getParameters
preset.setParameter
transport.get
transport.setTempo
transport.setTimeSignature
transport.start
transport.stop
input.listDevices
input.attach
input.detach
input.getMapping
input.setMapping
audio.listDevices
audio.selectDevice
audio.getLatency
performance.noteOn
performance.noteOff
performance.control
recording.start
recording.stop
recording.list
recording.load
recording.save
playback.start
playback.stop
playback.seek
config.get
config.set
diagnostics.selfTest
diagnostics.doctor
diagnostics.benchmark
```

### 16.4 实时 Wire 协议

为高频演奏事件定义固定、版本化、长度明确的二进制 `MolWireEventV1`：

- 明确 magic/version/type/size/sequence/timestamp；
- 使用显式 little-endian 编解码函数，不依赖结构体裸 memcpy；
- 可承载 note、control、pitch bend、all notes off；
- 可运行在 WebSocket binary、串口、BLE GATT 或测试管道；
- 解析器必须限制长度并进入 fuzz 测试；
- JSON-RPC 用于控制，Wire 协议用于高频事件。

### 16.5 `molctl`

至少实现：

```text
molctl status
molctl capabilities
molctl devices input
molctl devices output
molctl input attach <id>
molctl output select <id>
molctl preset list
molctl preset set grand-piano
molctl note on 60 --velocity 0.8
molctl note off --gesture <id>
molctl sustain on
molctl tempo 120
molctl chord major
molctl arpeggiator up --rate 1/16
molctl record start
molctl record stop --output song.molseq
molctl play song.molseq
molctl render song.molseq --output song.wav
molctl all-notes-off
molctl doctor
molctl self-test
molctl benchmark
```

CLI 输出默认适合人阅读，并提供 `--json` 供自动化使用。

---

## 17. 录音、回放和文件格式

### 17.1 录音原则

录制的是经过音乐处理后真正进入声部分配的音乐事件和必要控制变化，而不是简单录制麦克风或最终 PCM。

必须能够重现：

- 实际发声音符；
- velocity；
- sustain；
- pitch bend；
- 音色变化；
- 参数变化；
- tempo/time signature；
- transport；
- 随机琶音种子；
- 必要的初始状态。

### 17.2 Mol Sequence Format v1

定义 `.molseq` 紧凑二进制格式：

```text
magic
format version
header length
sample rate / time base
initial state
varint delta-frame events
optional metadata chunks
CRC32
```

要求：

- 流式读取；
- 固定上限；
- 可在 ESP32 上写入；
- 写入中断后能够识别损坏而不崩溃；
- 同一事件序列输出确定；
- 未知可跳过 chunk 有明确规则；
- parser/writer 进入 fuzz、round-trip 和 truncation 测试。

### 17.3 文本格式和转换

提供：

```text
.molseq.json  人类可读调试/交换格式
.mid          MIDI 导入导出
.wav          音频渲染输出
```

`mol-seq` 支持：

```text
inspect
validate
json-to-binary
binary-to-json
midi-import
midi-export
trim
merge
quantize（非破坏性，显式调用）
```

### 17.4 持久化

- 桌面：用户配置目录；
- Web：IndexedDB，必要时 OPFS；
- Android/iOS/HarmonyOS：应用私有目录；
- ESP32：NVS 保存设置，LittleFS/FAT 保存序列；
- 使用原子写入或临时文件 + rename；
- 配置损坏时回退默认并保留诊断，不阻止应用启动；
- 不默认上传任何儿童使用数据或录音。

---

## 18. Web UI 和可选图形界面

### 18.1 UI 技术路线

完整参考 UI 使用：

```text
TypeScript strict mode
Web Components / standards-based DOM
Canvas 2D for keyboard visualization
CSS responsive layout
Vite only as development/build tool
```

禁止把 React、Vue、Angular、Electron、Flutter 或大型 UI 框架设为项目核心依赖。

同一 Web UI 支持三种后端：

1. **Standalone Web**：直接控制 AudioWorklet 中的 `mol_core.wasm`；
2. **Native Service Controller**：通过本地 WebSocket 控制 `mol-keyboardd`；
3. **ESP32 Controller**：通过设备提供的受限 WebSocket/Wire 协议控制固件。

### 18.2 UI 模式

#### Explore Mode

默认儿童界面，只显示：

- 30 键视觉键盘；
- 音色切换；
- 音阶切换；
- 八度切换；
- 录音、停止、回放；
- 节拍器开关；
- 主音量。

#### Studio Mode

显示：

- Chorus、Delay、Reverb；
- 和弦类型；
- 琶音方向、速率、gate、八度；
- BPM、拍号；
- Portamento；
- 音色参数；
- 输入和输出设备；
- 诊断统计。

### 18.3 UI 行为

- 桌面展示完整 30 键；
- 手机横屏优先演奏；
- 手机竖屏展示可滑动局部音域，不把所有键压缩到不可用宽度；
- Pointer ID 映射独立 `gesture_id`；
- 支持多点触摸；
- pointer cancel、blur、pagehide、visibility hidden 释放相应手势；
- 和弦/琶音显示实际发声音符；
- 状态不只用颜色表达；
- 支持减少动画；
- 控件具备 ARIA 标签和键盘可达性；
- 不包含广告、账号、追踪脚本和第三方 CDN；
- 中英文可切换，默认跟随系统。

### 18.4 移动和 HarmonyOS 外壳

- Android 可使用系统 WebView 加载打包在应用内的同一 UI 资源；
- iOS 可使用 WKWebView；
- HarmonyOS 可使用 ArkUI Web 组件，必要时关键控件使用原生 ArkUI；
- 不加载远程页面；
- JS/Native Bridge 只开放版本化白名单方法；
- 所有消息做 Schema、长度和范围验证；
- WebView 只负责 UI，实时 PCM 由原生音频宿主生成；
- UI 被销毁后，后台音频运行时仍可按平台规则继续。

---

## 19. ESP32 具体目标

### 19.1 芯片配置

至少提供：

```text
ESP32 reference target
  - Classic Bluetooth + BLE
  - HID Host
  - A2DP Source
  - I2S

ESP32-S3 reference target
  - BLE
  - USB HID Host where hardware supports it
  - I2S
  - no Classic Bluetooth A2DP
```

### 19.2 ESP-IDF 工程

- `mol_core` 可作为独立 ESP-IDF component；
- 不修改核心源代码即可在 ESP-IDF 构建；
- 提供 `idf.py set-target esp32` 和 `esp32s3` 的脚本；
- 提供 `sdkconfig.defaults`；
- 不把本机绝对路径写入工程；
- 不要求 PSRAM 才能运行 Tiny 基线；
- 检测到 PSRAM 时可提高复音和效果长度。

### 19.3 任务模型

```text
High-priority audio render task
I2S/A2DP output task or callback
HID/GPIO input task
control/config task
optional network/UI task
```

要求：

- 音频任务不等待网络和存储；
- 网络栈阻塞不导致音频中断；
- 输入事件通过有界队列；
- 看门狗配置合理；
- 记录队列溢出、音频欠载、CPU 峰值和内存水位。

### 19.4 GPIO 键盘

- 配置化行列矩阵；
- 5–20 ms 可调去抖；
- 支持二极管矩阵和无二极管 ghosting 策略；
- 支持 velocity-less 固定力度；
- 可选电容触摸适配器；
- 板级文件定义 GPIO，不进入核心。

### 19.5 设备配置

- 长按指定配置键可进入配置模式；
- 可选 Wi-Fi AP 提供本地 Web 配置；
- 默认不暴露公网服务；
- 凭证存储使用 NVS；
- 固件恢复默认和清除配对有明确物理操作；
- 配置模式不会阻塞音频任务。

### 19.6 ESP32 验收预算

Tiny/ESP32 基线目标：

- 核心 + 内置音色工作内存不高于 256 KiB，目标更低；
- 核心 + 内置音色代码/只读数据不高于 512 KiB，生成 map 报告；
- 不依赖 PSRAM；
- 至少 8 声部，目标 12；
- 连续演奏 30 分钟无 watchdog reset 和音频 underrun；
- 18 音色均可选择并发声；
- I2S 为强制验收；
- A2DP 仅对支持经典蓝牙的目标验收；
- 与桌面运行同一 `.molseq` 时，音符事件顺序完全一致。

若真实数据证明预算不合理，必须先提供 map、profile 和基准证据，再通过 ADR 调整；不得未经测量直接放宽。

---

## 20. 后台服务与蓝牙场景验收

### 20.1 桌面无 UI 场景

在 Windows、Linux、macOS 至少验证：

```text
1. 系统已配对蓝牙音箱和蓝牙键盘。
2. 启动 mol-keyboardd，不打开图形界面。
3. 服务枚举或跟随系统蓝牙音频输出。
4. 服务连接可访问的物理键盘输入适配器。
5. 按键产生声音。
6. molctl 可切换音色、录音和回放。
7. 浏览器 UI 未启动时功能仍然完整。
```

权限和系统限制必须通过 `molctl doctor` 明确说明。

### 20.2 Android/HarmonyOS 后台场景

```text
1. 用户在前台明确启动音频运行时。
2. 应用建立合法前台服务/长时任务通知。
3. 切换到其他应用或锁屏。
4. 已经开始的演奏回放、节拍器或序列继续输出。
5. 音频焦点丢失时按策略暂停、duck 或停止。
6. 回到应用后状态一致。
```

普通硬件键盘按键在应用完全后台时不设为跨设备硬门槛；运行时 capability 和文档必须如实说明。

### 20.3 iOS 后台场景

```text
1. 用户启动音频功能。
2. AVAudioSession 配置和背景音频能力生效。
3. 锁屏或切后台后，正在运行的序列/持续音频按规则继续。
4. 来电或其他音频中断时正确响应。
5. 中断结束后根据用户意图恢复。
6. 无活动音频时不滥用后台执行。
```

### 20.4 ESP32 独立场景

```text
1. ESP32 无 PC、手机和 UI 常驻连接。
2. 蓝牙/USB/GPIO 键盘输入可用。
3. I2S 或支持的 A2DP 音频输出可用。
4. 音色、延音、和弦、琶音、节拍器、录音和回放由设备独立完成。
5. 重启后恢复持久设置。
```

---

## 21. 配置、能力与诊断

### 21.1 能力查询

运行时必须明确报告：

```text
build profile
sample rates
channel counts
max voices
available effects
sampler support
MIDI support
HID support
GPIO support
background audio status
low-latency requested/actual
A2DP source support
Web SharedArrayBuffer fast path
persistent storage status
UI availability
```

### 21.2 配置

配置文件使用严格 Schema。配置至少包括：

- 默认音色；
- 主音量；
- 采样率策略；
- 复音数；
- 输入映射；
- 输出设备 ID；
- 音阶和和弦；
- BPM；
- 服务 IPC；
- Web UI 是否启用；
- 日志级别；
- ESP32 板级配置引用。

不认识的字段按版本策略处理；无效值给出精确错误，不静默截断。

### 21.3 `doctor`

`molctl doctor` 必须检查：

- 核心版本和 ABI；
- 音频设备和实际格式；
- 低时延实际状态；
- Bluetooth 输出是否由系统暴露；
- HID/MIDI 权限；
- 服务 IPC；
- 配置和音色包；
- 存储可写；
- underrun 和 dropped command；
- Web 部署所需 HTTPS/cross-origin isolation；
- ESP32 capability 与芯片不匹配；
- 已知平台限制。

输出必须可操作，避免只打印“初始化失败”。

---

## 22. 安全、隐私和儿童产品原则

- 默认完全离线；
- 不要求账号；
- 不包含分析、广告、遥测和远程追踪；
- 不上传录音、按键、设备标识或儿童数据；
- 网络控制默认只绑定 loopback；
- ESP32 配置服务默认只在显式配置模式开放；
- 不把 token、密码和证书提交到仓库；
- 解析所有外部文件时限制大小、深度、数量和事件时长；
- 对 Patch、Sequence、RPC、Wire parser 进行 fuzz；
- 本地 Web UI 实施 Origin 和 token 检查；
- Native WebView 禁止任意导航和任意 JavaScript 接口；
- 发生异常时默认静音并释放音符；
- 主输出有数字限幅和默认保守增益；
- README 不作医疗、听力保护或绝对低时延承诺。

创建 `PRIVACY.md`，以清晰语言说明默认无数据收集。

---

## 23. 测试体系

### 23.1 单元测试

必须覆盖：

- MIDI note/frequency 转换；
- 30 键映射；
- 八度和移调；
- 全部音阶和主音；
- 全部和弦；
- 琶音模式和跨块调度；
- sustain 状态机；
- gesture 生命周期；
- voice stealing；
- ADSR；
- oscillator frequency；
- filter stability；
- effect parameter bounds；
- limiter；
- patch/sequence 编解码；
- config validation；
- queue wrap-around；
- reset/shutdown；
- 多实例。

### 23.2 属性和随机测试

验证：

- 任意合法命令序列不产生 NaN/Inf；
- 所有活动 gesture 最终可释放；
- `ALL_SOUND_OFF` 后输出在有限斜坡后为静音；
- 随机参数不会让滤波器失稳；
- 序列 encode/decode round-trip；
- patch compiler 确定性；
- 不同 block size 渲染结果在容差内一致。

### 23.3 Fuzz

至少建立：

```text
fuzz_patch_parser
fuzz_sequence_parser
fuzz_config_parser
fuzz_jsonrpc_dispatch
fuzz_wire_parser
fuzz_midi_parser
```

使用 sanitizer 运行，保存最小复现语料。

### 23.4 音频自动分析

`tools/audio-analyze` 检查：

- 非静音；
- peak/RMS；
- NaN/Inf；
- clipped samples；
- 基频误差；
- 包络 attack/release；
- DC offset；
- 谱能量；
- 静音尾部；
- 左右声道；
- effect impulse response；
- 点击/突变候选。

### 23.5 Golden Audio

- 固定序列、固定随机种子、固定 48 kHz；
- 同一平台同一构建应字节稳定或哈希稳定；
- 跨架构不强制 float PCM 字节相同，比较事件流、频谱、包络和误差容差；
- Golden 更新必须通过显式工具并记录原因；
- 不允许为了让测试通过自动覆盖 Golden。

### 23.6 Native/Wasm 一致性

同一序列在 Native 和 Wasm 上验证：

- 实际音符事件完全一致；
- transport frame 完全一致；
- voice steal 决策一致；
- PCM 指标在定义容差内；
- 随机琶音在同种子下完全一致。

### 23.7 长时间测试

至少包括：

- 2 小时持续 transport；
- 30 分钟高密度随机音符；
- 反复切换 18 音色；
- 反复设备断开和恢复；
- 反复 start/stop recording；
- 队列接近满载；
- Web AudioContext suspend/resume；
- 移动端前后台切换；
- ESP32 30 分钟真实播放。

### 23.8 覆盖率

- 第一方核心目标行覆盖率不低于 90%；
- 音乐状态机、Patch、Sequence、队列和内存模块目标不低于 95%；
- 平台 glue 不用虚假 Mock 覆盖率代替真实集成测试；
- 覆盖率下降必须在 CI 阻止。

---

## 24. 性能和体积门槛

### 24.1 实时音频

Standard 档位在合理现代设备上：

- 48 kHz stereo；
- 32 声部全功能稳定；
- 目标 64 声部；
- 无每块动态分配；
- 连续 30 分钟无 underrun；
- 音频线程平均占用目标低于单核 25%；
- 峰值不导致 callback deadline miss。

### 24.2 端到端延迟

延迟必须真实测量并区分：

```text
内置/有线输出
USB 音频
系统蓝牙音频
Web Audio
ESP32 I2S
ESP32 A2DP
```

门槛：

- 代表性内置/有线设备目标 P95 ≤ 30 ms，验收上限 50 ms；
- Web 在支持良好的桌面浏览器目标 P95 ≤ 50 ms；
- Bluetooth 只记录实测，不套用 50 ms 硬门槛；
- 测量方法、设备和缓冲配置写入报告。

### 24.3 二进制体积

目标：

- stripped `mol_core` 原生库小于 1 MiB；
- 无 UI 的 daemon + CLI 除系统动态库外目标小于 5 MiB；
- 标准 Web 音频 Wasm 压缩后目标小于 1.5 MiB；
- Web UI 首次核心资源目标小于 2 MiB，不含可选音色样本；
- ESP32 预算见专门章节。

超过目标时必须提供 size/map 分析，不得无说明扩大。

---

## 25. 依赖和许可证规则

### 25.1 允许原则

优先：

```text
MIT
BSD-2-Clause
BSD-3-Clause
Apache-2.0
Zlib
ISC
CC0
OFL（仅字体）
```

禁止：

- AGPL；
- 未经批准的 GPL 运行时依赖；
- 未经批准的 LGPL 静态链接依赖；
- 来源不明代码；
- 来源不明 SoundFont、采样和字体；
- 需要商业许可证才能分发的核心组件；
- 从旧 MoL Keyboard 复制代码。

### 25.2 依赖策略

运行时依赖应最小化。预期候选仅包括：

- miniaudio：桌面音频宿主；
- Oboe：Android 音频；
- CivetWeb 或经审计的轻量 MIT WebSocket/HTTP 库：桌面本地控制；
- 平台官方 SDK；
- 测试框架和开发工具。

核心 DSP、音乐逻辑、Patch、Sequence 和事件队列不得依赖大型框架。

### 25.3 引入流程

每个第三方组件引入前必须：

1. 固定官方来源和精确版本/提交；
2. 保存 LICENSE 快照；
3. 检查仓库子目录和捆绑组件许可证；
4. 记录启用的构建选项；
5. 记录本地 patch；
6. 生成 `third_party/manifest.lock.json`；
7. 在 CI 中运行许可证审计；
8. 生成 SPDX 或 CycloneDX SBOM；
9. 不得仅根据项目首页或记忆判断许可证。

---

## 26. 代码质量规范

- C 核心使用 ISO C11；
- 宿主层可使用 C++17，但公共边界仍为 C ABI；
- C/C++ 格式基于 Google 风格，统一 `.clang-format`；
- 公共 API 使用 Doxygen；
- 标识符、注释、Doxygen、提交信息和开发文档使用英文；
- README 和用户文档提供中英文；
- 不在注释中记录聊天过程、临时推理或“AI generated”；
- 不保留无用、重复、未引用和注释掉的大段代码；
- 平台宏集中在平台适配层；
- 禁止静默吞掉错误；
- 禁止 `assert` 代替外部输入校验；
- Release 中也保留必要参数校验；
- 所有编译器警告视为错误；
- 使用 clang-tidy、ASan、UBSan、TSan（适用模块）和静态分析；
- 关键算术检查整数溢出和尺寸乘法；
- 生成代码和第三方代码与第一方代码明确分离；
- 文件使用 LF 和 UTF-8；
- 公共头文件可独立包含并通过 C、C++ 编译测试。

---

## 27. Git 与提交规则

- 默认分支策略：在 `codex/mol-keyboard-v1` 完成开发；
- 提交信息使用英文 Conventional Commits；
- 每个提交只完成一个清晰目标；
- 第一方手写代码单提交原则上不超过 500 行净变更；
- 生成文件、锁文件、平台模板和第三方导入可例外，但必须独立提交并说明；
- 不把格式化全库和功能修改混在一个提交；
- 每个提交前运行与修改相关的最小完整测试；
- 每个里程碑前运行完整回归；
- 不提交 build、缓存、签名、密钥、设备日志和本地绝对路径；
- 不通过 amend 或 rebase 改写用户已有提交；
- 状态文档更新与对应功能放在同一提交或紧随其后的文档提交。

推荐提交序列示例：

```text
chore: initialize portable C11 project
feat(core): add arena-based engine lifecycle
feat(dsp): implement band-limited oscillators
feat(music): add scale and chord processing
feat(runtime): add bounded command queue
feat(desktop): add headless audio service
feat(web): run engine in AudioWorklet
feat(esp32): add I2S audio host
...
```

---

## 28. 文档交付

必须完成：

```text
README.md
  - 项目故事
  - 产品定位
  - 快速开始
  - 无 UI 使用
  - Web 使用
  - 平台矩阵
  - 键位图
  - 构建说明
  - 限制

ARCHITECTURE.md
  - 层次、数据流、线程、内存、时间

ABI_POLICY.md
REALTIME_SAFETY.md
DSP_DESIGN.md
MUSIC_SEMANTICS.md
PATCH_FORMAT.md
SEQUENCE_FORMAT.md
WIRE_PROTOCOL.md
SERVICE_PROTOCOL.md
PLATFORM_PORTING.md
ESP32_PORT.md
WEB_AUDIO.md
ANDROID_AUDIO.md
APPLE_AUDIO.md
HARMONY_AUDIO.md
TEST_PLAN.md
BENCHMARKS.md
PRIVACY.md
ASSET_LICENSES.md
THIRD_PARTY_NOTICES.md
RELEASING.md
```

关键架构决策使用 ADR，至少包括：

```text
ADR-0001 C11 portable core
ADR-0002 caller-provided memory
ADR-0003 audio-frame scheduling
ADR-0004 procedural baseline instruments
ADR-0005 optional web UI
ADR-0006 platform-native audio hosts
ADR-0007 ESP32 capability split
ADR-0008 sequence and patch formats
```

---

## 29. 质量门禁和里程碑

### M0 — 空白仓库与工程基线

完成：

- Git、许可证、README 初稿；
- CMake Presets；
- `mol_core` 空操作但真实可链接的最小生命周期；
- warnings-as-errors；
- 单元测试框架；
- Linux/Windows/macOS 构建入口；
- Emscripten 构建入口；
- ESP-IDF component 构建入口；
- 状态文档；
- CI 基础矩阵。

验收：

- `mol_core` 不包含平台头文件；
- C 和 C++ consumer 都可链接；
- native、Wasm 和 ESP-IDF 至少完成实际编译，缺失的工具链不得假装通过。

### M1 — 最小跨平台声音纵向链路

完成：

- arena 初始化；
- command；
- 一个 band-limited oscillator；
- ADSR；
- 8 声部；
- WAV offline sink；
- 桌面实时输出；
- Web AudioWorklet；
- ESP32 I2S；
- Android/Apple/Harmony 工程调用入口。

验收：

- 同一 C4 命令序列在 Native、Wasm 和 ESP32 核心产生正确频率；
- 无 NaN/Inf；
- 音频回调无分配；
- 这一步是架构 Gate，不允许先写完整 UI 再回头验证可移植性。

### M2 — 完整音乐语义

完成：

- 30 键映射；
- 八度和移调；
- scale lock；
- chord mode；
- sustain；
- portamento；
- transport；
- metronome；
- arpeggiator；
- gesture ownership；
- sample-accurate 调度；
- 完整单元和属性测试。

验收：

- 所有组合场景无卡音；
- 同一序列的事件流跨 Native/Wasm 一致；
- 2 小时 transport 无累计漂移超限。

### M3 — DSP、18 音色和效果器

完成：

- 全部 DSP 模块；
- 18 个数据驱动过程音色；
- Patch compiler；
- Chorus、Delay、Reverb；
- mixer、limiter；
- 音色响度校准；
- golden audio 和音频分析。

验收：

- 18 音色均非空、可区分、无数值错误；
- 快速切换 18 音色无卡音和明显爆音；
- Tiny 构建包含全部音色 ID；
- ESP32 预算有真实 map 证据。

### M4 — 录音、回放和离线工具

完成：

- `.molseq`；
- JSON 转换；
- MIDI 导入导出；
- 事件录音；
- 确定性回放；
- WAV 离线渲染；
- parser fuzz；
- 示例作品。

验收：

- round-trip；
- 损坏文件安全失败；
- 同一序列多次回放事件一致；
- Native/Wasm/ESP32 读取相同基础序列。

### M5 — 桌面无界面产品

完成：

- `mol-keyboardd`；
- `molctl`；
- miniaudio 设备；
- Windows/Linux/macOS 输入适配；
- local IPC；
- device hotplug；
- doctor/self-test/benchmark；
- 用户级后台启动配置。

验收：

- 无 UI 演奏；
- 蓝牙音箱作为系统输出时可工作；
- 可访问的物理键盘输入可工作；
- CLI 完成音色、效果、录音和回放；
- 服务退出无残留音符和资源。

### M6 — Web/PWA 完整产品

完成：

- full Web UI；
- AudioWorklet Wasm；
- MessagePort 基线；
- SharedArrayBuffer 快速路径；
- IndexedDB/OPFS；
- PWA 离线；
- keyboard/touch；
- service-controller 模式；
- 浏览器自动化测试。

验收：

- Chrome、Edge、Firefox、Safari 当前稳定版验证；
- 桌面和移动浏览器合理覆盖；
- 主线程繁忙时音频仍稳定；
- 页面失焦无卡音；
- 基础构建不依赖 cross-origin isolation。

### M7 — Android 与 iOS

完成：

- Android Oboe + foreground service + UI shell；
- iOS AudioUnit/AVAudioSession + UI shell；
- 共享 Web UI 资源或等价完整界面；
- 物理键盘前台输入；
- 路由、中断、后台音频；
- 应用打包和隐私文档。

验收：

- 模拟器 build-verified；
- 真机 device-verified；
- 退后台/锁屏测试；
- 蓝牙输出遵循系统路由；
- 不对后台普通键盘输入作虚假承诺。

### M8 — HarmonyOS

完成：

- ArkUI/ArkTS shell；
- Node-API；
- OHAudio；
- low-latency status；
- audio focus；
- continuous task；
- UI 和本地存储；
- HAP 打包。

验收：

- DevEco 真实构建；
- 真机实时演奏；
- 后台音频；
- 输出路由变化；
- 无 ArkTS PCM 生成。

### M9 — ESP32 完整设备

完成：

- ESP32 I2S；
- ESP32-S3 I2S；
- GPIO matrix；
- BLE/Classic/USB HID 的适用实现；
- 原始 ESP32 A2DP Source；
- NVS/LittleFS；
- 配置模式；
- 可选设备 Web UI；
- HIL 测试和文档。

验收：

- 两类芯片真实硬件验证；
- 30 分钟无 underrun/watchdog；
- 无 PSRAM Tiny 基线；
- 18 音色和完整音乐功能；
- A2DP capability 只在支持芯片出现。

### M10 — v1.0.0 发布门禁

完成：

- 所有强制测试；
- 核心覆盖率；
- sanitizer/fuzz；
- platform matrix；
- 性能和体积报告；
- LICENSE、NOTICE、SBOM；
- 中英文 README；
- 安装包/构建产物；
- release notes；
- 示例和截图；
- clean checkout 复现构建。

只有满足本文件 Definition of Done 后才能标记 v1.0.0。

---

## 30. Definition of Done

项目只有同时满足以下条件才算完整：

1. 从空白目录形成结构清晰的 Git 仓库。
2. `mol_core` 为无平台依赖 ISO C11，支持调用方提供内存。
3. 核心可在 Native、Wasm 和 ESP-IDF 使用同一源码构建。
4. 30 键、八度、音阶、和弦、琶音、延音、滑音、节拍器全部正确。
5. 18 个内置音色全部真实可演奏，不是占位实现。
6. Chorus、Delay、Reverb 和 Limiter 可用且实时安全。
7. 录音、回放、`.molseq`、MIDI 转换和 WAV 渲染可用。
8. Windows、Linux、macOS 无 UI 服务和 CLI 可用。
9. Web/PWA 使用 AudioWorklet + Wasm，可离线运行。
10. Android 使用 Oboe 并支持合法后台音频。
11. iOS 使用 AudioUnit/AVAudioSession 并支持合法后台音频。
12. HarmonyOS 使用 OHAudio 和官方长时任务机制。
13. ESP32/ESP32-S3 使用 I2S；支持芯片上实现 A2DP Source。
14. 物理键盘、触摸、MIDI、GPIO 和程序输入统一进入同一事件模型。
15. 蓝牙音箱和键盘能力按平台事实实现并正确报告限制。
16. 音频回调无动态分配、阻塞、日志和跨语言回调。
17. 所有 parser 有边界测试和 fuzz。
18. 核心覆盖率、sanitizer、静态分析和长时间测试通过。
19. 所有依赖固定、许可清楚、SBOM 完整。
20. clean checkout 按文档可复现主要构建。
21. 平台支持声明均有真实证据，没有“理论支持”。
22. 不包含旧 MoL Keyboard 代码和来源不明资源。
23. README 完整讲明“张多少的键盘 / More or Less Zhang's Keyboard”的故事与定位。
24. 发布 `v1.0.0`，CHANGELOG 和 release notes 与真实功能一致。

---

## 31. 首次执行的强制顺序

首次读取本文件后，按以下顺序直接开始：

1. 检查目录和 Git 状态；
2. 初始化仓库和 `codex/mol-keyboard-v1` 分支；
3. 创建最小 README、LICENSE、状态文档和构建骨架；
4. 完成 `mol_core` arena 生命周期和 C consumer 测试；
5. 完成 WAV 离线正弦波纵向链路；
6. 编译同一核心到 WebAssembly；
7. 建立 ESP-IDF component 并真实编译；
8. 建立 AudioWorklet 和 ESP32 I2S 最小发声；
9. 通过 M1 Gate 后再扩展完整音乐语义；
10. 持续推进 M2–M10，直至 Definition of Done。

不得首先投入大量时间制作界面、图标、宣传页或平台空壳。第一优先级始终是证明同一个轻量核心可以在原生、WebAssembly 和 ESP32 上正确发声。

---

## 32. 持续执行与恢复规则

每次工作结束前必须：

1. 运行与本次修改相关的真实构建和测试；
2. 更新 `docs/status/IMPLEMENTATION_STATUS.md`；
3. 记录验证命令、结果和剩余最高优先级；
4. 提交已完成的最小原子成果；
5. 保持工作区干净，或明确记录未提交内容原因；
6. 下一次继续时从尚未通过的最高质量门禁开始。

`IMPLEMENTATION_STATUS.md` 至少包含：

```text
Current milestone
Last verified commit
Completed requirements
In-progress work
Blocked platform checks
Exact validation commands
Known failures
Next highest-priority task
```

不要重复已经有证据完成的工作，不要因一次 Codex 会话结束而重新设计项目，也不要用新的“计划文档”替代实际代码。

---

## 33. 最终交付物

最终仓库至少产出：

```text
Libraries
  mol_core static/shared library
  public C headers
  optional C++ wrapper

Desktop
  mol-keyboardd
  molctl
  mol-render
  local Web UI bundle

Web
  mol_core.wasm
  AudioWorklet module
  installable PWA

Android
  installable debug/release-ready project
  native Oboe runtime

Apple
  macOS host
  iOS application project
  native Core Audio runtime

HarmonyOS
  HAP project
  OHAudio runtime

ESP32
  ESP-IDF component
  ESP32 reference firmware
  ESP32-S3 reference firmware

Tools
  mol-patchc
  mol-seq
  audio analyzer
  latency probe
  license/SBOM tools

Documentation
  complete architecture, API, porting, testing and release docs

Evidence
  tests
  coverage
  fuzz results
  benchmarks
  platform matrix
  size reports
  hardware validation reports
```

**从现在开始直接实现，不再重新讨论是否重写、是否需要兼容旧代码或是否采用 UI-first 架构。项目是一个完全新的、以轻量 C11 核心为根、无界面优先、平台适配分离、最终覆盖桌面、Web、移动、HarmonyOS 和 ESP32 的 MoL Keyboard。**
