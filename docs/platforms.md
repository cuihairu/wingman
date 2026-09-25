# 平台支持与 API 适用性

> 最后核对：2026-09-24（v18 覆盖率基线同期）。本文是平台信息的**唯一对账表**：
> 当 README 的承诺、CI 实际验证的范围、代码条件编译的真实覆盖三者出现不一致时，
> 以本文记录的现状为准，并在此修订。

## 为什么单独一篇平台文档

Wingman 的平台信息此前散在三处：README 的徽章（badge）宣称"支持 Windows、macOS、Linux"，
CI 配置定义了"每个提交实际被哪些平台验证"，代码里的条件编译（Conditional Compilation）
决定了"每个功能在哪个平台真的有后端"。这三者各自演进、互相不加校验——README 写了的
CI 未必测，CI 编译了的功能未必在所有平台有实现，而用户最终只能通过一篇集中的文档
回答"我要的平台能不能用、不能用是为什么"。

按平台分文档的本质是把三条线对齐：

1. **用户按平台找信息**——支持矩阵是入口，API 适用性清单是答案；
2. **CI 按平台验证**——[CI 验证对应](#ci-验证对应)一节把每个 job 与矩阵条目一一对应，
   哪个承诺没有 CI 兜底必须如实可见（见该节的诚实标注）；
3. **代码按平台条件编译**——平台宏只允许出现在平台租户目录
   （`lib/wingman/src/platform/`，由 `scripts/check_platform_boundary.sh` 强制，
   见 [架构决策 · Platform Macro Boundary](architecture-decisions.md)），
   这保证了"一个平台有没有某能力"永远可以在租户目录里找到唯一事实源。

## 平台支持矩阵

| 平台 | 状态 | 架构 | 最低版本要求 | 获取方式 | 已知限制 |
|------|------|------|--------------|----------|----------|
| Windows | 已支持 | x86_64 | Windows 10 / Server 2016+ | [Nightly](https://github.com/cuihairu/wingman/releases/tag/nightly) 或 Release 的 `wingman-*-windows-x64.zip`；源码构建见 [BUILD](../BUILD.md)（vcpkg 三元组 `x64-windows-static`） | OCR / ML / Python 引擎为可选开关，默认关闭（见 [API 适用性](#api-适用性标注)） |
| Linux | 已支持 | x86_64 | Ubuntu 22.04+（GCC 11+），X11 显示服务 | Nightly / Release 的 `wingman-*-linux-x64.tar.gz`（含 AppImage 与 deb）；源码构建（三元组 `x64-linux`） | 仅 X11；Wayland 需经 XWayland 兼容层，原生 Wayland 未支持；无 UI 自动化后端（见[环境差异](#各环境差异说明)） |
| macOS | 已支持 | x86_64（Intel） | macOS 12+（部署目标 `MACOSX_DEPLOYMENT_TARGET=12`） | Nightly / Release 的 `wingman-*-macos-x64.tar.gz`（含 dmg 与 app）；源码构建（三元组 `x64-osx`） | Apple Silicon（arm64）原生构建未提供；无 OpenCV（视觉分析走内置路径）；CI 验证为尽力而为（continue-on-error） |
| Android | 实验性 | arm64 | Android 9.0+（minSdk 28，targetSdk 34） | Nightly / Release 的 `wingman-*-android-arm64` APK（debug 签名）；源码构建见 [apps/android/README.md](../apps/android/README.md) | 仅 A2 能力面（`wingman.input/screen/vision` 三张 Lua 子表，见下文）；无正式签名；生命周期保活与安全加固属 A3 规划 |
| iOS | 规划中 | arm64 | — | — | 端侧 Agent 不可行（沙箱限制），仅"主机控"路线有设计，见 [iOS 一节](#ios-为什么是-规划中-而不是-支持) |

获取方式的补充说明：Nightly 每日构建自 main 分支，**仅保留当日最新一组**（按提交 sha
命名，形如 `2026.09.24-nightly-<sha>`）；Release 由 tag 触发，产物经 SHA256SUMS 校验。
两者均为预发布性质的产物，正式版本号目前停在 v0.1.x（见 [CHANGELOG](../CHANGELOG.md)）。

## API 适用性标注

**这是本篇文档的核心用途**：Wingman 尚未做到跨平台 API 完全统一——同一功能名在不同
平台可能"有真实后端 / 有降级实现 / 完全不存在"。下表让用户一眼知道自己所在平台能不能用。

标注含义：✅ 完整支持 · ⚠️ 受限/降级（附原因） · ❌ 不可用（附原因） · 📋 规划中（iOS）。

### 脚本 API（Lua `wingman.*` 模块，桌面 Runtime）

桌面 Runtime 的脚本 API 按"平台绑定 → 纯逻辑 → 可选能力"三组组织。之所以这样分组，
是因为三组**不可用的原因不同**：平台绑定组取决于目标系统有没有对应系统能力（比如
Linux 没有 UI Automation 框架），纯逻辑组理论上处处可用，可选能力组取决于构建时
有没有开启对应依赖开关。

**平台绑定组**（每个功能依赖一个平台后端，后端在 `lib/wingman/src/platform/` 租户目录）：

| 功能点 | Windows | Linux | macOS | Android | iOS |
|--------|---------|-------|-------|---------|-----|
| `wingman.screen`（截屏/取色/区域查询） | ✅ GDI | ✅ X11 | ✅ Cocoa | ⚠️ 经 A 线子表（见下文） | 📋 WDA 后端 |
| `wingman.input`（键鼠注入/移动轨迹） | ✅ SendInput | ✅ XTest | ✅ CGEvent | ⚠️ 触屏语义经 A 线子表 | 📋 WDA 后端 |
| `wingman.window`（窗口查找/激活/边界） | ✅ Win32 | ✅ X11 | ✅ Cocoa | ❌ 端侧无窗口管理器语义（无此需求） | 📋 |
| `wingman.clipboard`（剪贴板读写） | ✅ | ✅ X11 | ✅ Cocoa | ❌ 未实现（A 线无此 API 面） | 📋 |
| `wingman.filewatcher`（文件变更监控） | ✅ ReadDirectoryChangesW | ✅ inotify | ✅ FSEvents | ❌ 未实现 | 📋 |
| `wingman.process`（进程枚举/启动/终止） | ✅ Win32 | ✅ POSIX | ✅ POSIX | ❌ 未实现 | 📋 |
| `wingman.macro`（宏录制/回放） | ✅ 系统钩子 | ✅ XRecord | ✅ Cocoa 事件 | ❌ 未实现 | 📋 |
| `wingman.uiAutomation`（UI 元素树自动化） | ✅ UI Automation | ❌ **无后端**：Linux 无等价框架，调用降级返回失败并告警 | ✅ AXUIElement | ❌ 未实现 | 📋 |

**纯逻辑组**（不触碰平台系统能力，桌面三平台行为一致；Android 端 A 线脚本面未引入）：

| 功能点 | Windows / Linux / macOS | Android | 说明 |
|--------|-------------------------|---------|------|
| `wingman.event` / `fsm` / `behaviorTree` / `smartTrigger` | ✅ | ❌* | 事件总线、状态机、行为树、智能触发 |
| `wingman.task`（异步任务/重试/超时） | ✅ | ❌* | A 线有独立轻量执行模型，不走桌面模块注册表 |
| `wingman.transport`（TCP/UDP 收发） | ✅ | ❌* | Android 的网络面在 Kotlin 壳与 `libs/transport` 库层，不在 Lua 模块层 |
| `wingman.inbox` / `team` / `notify` / `orchestration` | ✅ | ❌* | 云控下行/组队/通知面，属桌面 Runtime 语义 |
| `wingman.http`（HTTP 客户端） | ✅ | ❌* | 依赖 libcurl，构建带 `WINGMAN_HAS_CURL` 时可用 |
| `wingman.json` / `ini` / `kv` / `db` / `config` | ✅ | ❌* | 序列化与存储面 |
| `wingman.crypto` / `security`（含 TOTP） | ✅ | ❌* | |
| `wingman.human`（拟人化轨迹）/ `gameProfile` / `verification` / `timer` / `util` / `system` | ✅ | ❌* | `system` 的信息采集部分依赖平台后端，三桌面均有 |

\* 标注"❌\*"的含义：不是能力缺失，而是**架构性不适用**——Android Agent（A 线）的
ScriptRunner 不加载桌面模块注册表（见
[Android 设计 · 5.5 的偏离说明](android-agent-design.md)），它只暴露下文三张子表。
把桌面全套模块搬上端侧是 A 线后续里程碑的候选方向，当前未承诺。

**可选能力组**（取决于构建开关与依赖可用性，默认关闭的功能不随产物分发）：

| 功能点 | Windows | Linux | macOS | Android | 不适用原因 |
|--------|---------|-------|-------|---------|------------|
| `wingman.vision` 之 **找色/找图/模板匹配**（`findColor`/`findImage` 等） | ✅ | ✅ | ✅ | ✅（A 线子表，bitmap-first 差异见下文） | 核心走内置 `ImageAnalyzer`（纯 C++ 逐像素实现，不依赖 OpenCV），全平台可用 |
| `wingman.vision` 之 **OpenCV 加速分析**（`WINGMAN_ENABLE_VISION`） | ✅ | ✅（vcpkg `vision` feature） | ❌ 无 OpenCV 依赖（manifest 未覆盖 macOS） | ❌ | macOS/Android 构建不带 OpenCV；需引入时走 vcpkg manifest，不手探系统库 |
| `wingman.ocr`（文字识别，`WINGMAN_ENABLE_OCR`） | ✅（Tesseract） | ❌ | ❌ | ❌ | vcpkg `ocr` feature 仅覆盖 Windows；无 OCR 时脚本调用降级返回空结果 |
| `wingman.ml`（ONNX 推理，`WINGMAN_ENABLE_ML`） | ✅（ONNX Runtime） | ❌ | ❌ | ❌ | 同上，`ml` feature 仅 Windows |
| Python 双引擎（`WINGMAN_ENABLE_PYTHON`） | ⚠️ 实验性 | ❌ | ❌ | ❌ | 仅 Windows 有 CI 实验 job；Linux/macOS 构建未验证 Python 引擎 |

### Android 端侧脚本面（A 线）

Android Agent 的 Lua 环境只有 `wingman` 下三张子表（A2 能力闭环，
见 [Android 设计 · §5.5/5.6](android-agent-design.md)）：

| 子表 | 能力 | 与桌面的行为差异 |
|------|------|------------------|
| `wingman.input` | `tap`/`swipe`/`longPress`/`text`/`delay`（经无障碍服务 Gesture API 注入） | 触屏语义（无悬停/右键/滚轮）；`delay` 可被停止标志中断 |
| `wingman.screen` | `captureFrame`（MediaProjection 采集）/ `screenSize` | 帧来自推送式 ImageReader 缓存，非按需截屏 |
| `wingman.vision` | `findColor`/`findImage` 等查找 | **bitmap-first**：帧取一次、多次分析，与桌面"每次查找重新截屏"是结构性差异，脚本编写时需注意时效性 |

桥未授权（无障碍/投屏权限未给）时函数降级返回 `false`/`nil` 而非抛错——脚本应检查
返回值自行兜底，这是 A 线的防御性约定。

### C++ 公开头文件层（嵌入/二开视角）

`lib/wingman/include/wingman/` 的公开头分三层，逻辑同上：

| 层 | 代表头文件 | 适用性 |
|----|-----------|--------|
| 平台抽象接口 | `platform/iscreen.hpp`、`iwindow.hpp`、`iinput.hpp`、`icapture.hpp`、`iclipboard.hpp`、`ifilewatcher.hpp` | 接口本身平台无关；实现存在性同"平台绑定组"。Android 仅有 `ICaptureSource` 适配（`platform/android/`） |
| 平台相关门面 | `screen.hpp`、`window.hpp`、`clipboard.hpp`、`filewatcher.hpp`、`process.hpp`、`recorder.hpp`、`system.hpp`、`ui_automation.hpp` | 同脚本 API 对应行；`ui_automation.hpp` 在 Linux 无后端 |
| 跨平台逻辑 | `event.hpp`、`trigger.hpp`、`behavior_tree.hpp`、`config.hpp`、`kvstore.hpp`、`crypt.hpp`、`storage/*`、`script/*`、`rpc/*` 等 | 桌面三平台一致；Android 复用其子集（`libs/agentcore`、`libs/androidagent`） |
| 平台限定 | `screenshot_reporter.hpp` | **仅 Windows 编译**（Windows 崩溃/截图上报链路专用） |

IPC 工厂（`ipc/ipc_factory.hpp`）按平台自动选择：Windows 用命名管道（Named Pipe），
Unix 系用 Unix 域套接字，均带 TCP 回退（`127.0.0.1:9800`）——嵌入方无需感知差异，
但跨平台联调时回退端口的防火墙放行要留意。

## 各环境差异说明

以下差异全部来自实际踩坑（CHANGELOG 有对应修复记录），不是理论差异罗列。
每条按"差异 → 为什么 → 对脚本/使用的影响"展开。

### 显示服务与截屏

- **Linux 只支持 X11**：截屏、取色、注入、宏录制（XRecord）全部建立在 X 协议上。
  原因是历史与生态——XTest/XRecord 是 Linux 桌面注入的事实标准，而 Wayland 出于
  安全模型刻意不提供全局注入通道。影响：Wayland 会话需切换到 XWayland 或 X11 会话；
  无头环境用虚拟显示（Xvfb）即可跑全量测试——CI 的 Linux 测试 job 与本机覆盖率采集
  都是这样执行的。
- **无显示环境的优雅降级**：Linux 无 `DISPLAY` 时，屏幕/窗口类查询返回降级值而非崩溃
  （有专门的宽容 X 错误处理器，BadWindow 等错误以降级返回值呈现——这是踩过"死句柄
  查询杀死整个进程"的坑后修的）。影响：脚本可以对无头环境做功能探测后自行分支。
- **macOS 采集后端可选**：ScreenCaptureKit 采集受编译开关（`HAVE_SCREENCAPTUREKIT`）
  控制。原因：该框架有系统版本门槛，关掉后退化到 Cocoa 基础路径仍能工作。
- **Android 采集是推送式**：MediaProjection 帧经 ImageReader 独立线程推送入缓存
  （见上表 bitmap-first 条目），与桌面"查询时截屏"的拉取式语义不同。这是权限模型的
  结果——Android 每次主动截屏都可能触发系统提示，推送式缓存规避了这一点。

### 权限与文件

- **root 下权限类防御失效**：`chmod 000` 对 root 无效（root 无视文件权限位），
  依赖"不可读文件"做降级测试/验证的场景在 root 环境行为不同。影响：以 root 运行
  Desktop 测试时部分权限分支不可达（测试中已用 `geteuid()` 显式跳过）。
- **特殊设备文件是 Linux 专属**：`/dev/full`（写入必满错误）、`/proc`（进程信息）等
  在 Windows/macOS 不存在，任何依赖它们的脚本或测试都需平台分支。
- **macOS 沙箱与辅助功能授权**：CGEvent 注入与宏录制需要"辅助功能"（Accessibility）
  授权，未授权时注入静默无效。原因：苹果把全局事件注入视为高权限操作。
  Windows 无此层（SendInput 默认可用），Linux XTest 依赖 X 会话授权。

### 进程与信号

- **信号语义不对称**：POSIX 有 `SIGPIPE`（对已关闭 socket 写入即杀进程），Windows 没有。
  历史上 IPC 双通道曾因未忽略 `SIGPIPE` 导致远端断开时进程直接死亡（已修复）。
  影响：二开者新增 socket 写入路径时必须沿用现有的 `MSG_NOSIGNAL`/忽略处理，不能只
  在 Windows 上测试通过就认为安全。
- **子进程模型不同**：POSIX 是 fork/exec 模型（`posix_process.cpp`），Windows 是
  CreateProcess 族（`win32_process.cpp`）。对脚本可见的行为差异：Windows 拿不到
  fork 语义（如写时复制的内存快照用法），Linux 的 `waitForever` 阻塞语义在 Windows
  由句柄等待实现，表现一致但实现路径完全不同。

### 剪贴板的异步性（Linux 特有）

Linux 上 `xclip` 每次 `setText`/`clear` 都会 fork 一个守护进程**异步**接管剪贴板
selection——写后立即读可能命中旧 owner（实测启动可超过 500ms）。Windows/macOS 的
剪贴板写入是同步生效的系统调用语义。影响：Linux 上"写后立即断言"的脚本要容忍短暂
延迟窗口；库内已用轮询封装收口，脚本层通常无感，但跨工具链混用剪贴板时要知道这层。

### IPC 与网络

- **IPC 通道三平台不同形**：Windows 命名管道 / Unix 域套接字 / TCP 回退，由工厂按
  平台选择。本地 UI 控制面遵守[架构决策](architecture-decisions.md)的本地 IPC 约束
  （Runtime 不开任何本地 HTTP/WebSocket 服务），因此 IPC 行为差异不会泄漏到远程面。
- **远程面只有一条**：所有平台的远程控制都经出站 TCP 连 Go Server（Runtime-as-Agent），
  Android 端侧亦然。这是刻意的架构约束，保证"平台差异"止步于执行面，不进入控制面。

## iOS：为什么是"规划中"而不是"支持"

iOS 的状态不是"还没做"，而是"端侧做不了"。App Store 沙箱（Sandbox）不允许跨 App
注入与全局截屏，**不存在合法的端侧执行通道**——这是 [可行性分析](mobile-support-feasibility.md)
的核心结论，也是本节所有判断的前提。

存在的唯一合法路线是"主机控"：iOS 设备作为被控外设，由桌面 Runtime 经 USB 通道
（usbmuxd 端口转发）连接设备上的 WebDriverAgent（WDA，Apple 官方测试框架的社区
打包），截屏与触摸注入走 XCTest 能力。该路线的完整设计见
[iOS 支持设计](ios-agent-design.md)（I1，仅设计未实现），其约束决定了它是"规划中"：

- **签名链无法自动化**：开发者账号 + 证书 + 描述文件（免费证书 7 天过期）+ 目标设备
  开发者模式 + Mac 执行 `xcodebuild` 部署 WDA，全部是人工前置步骤；
- **版本跟随成本**：WDA 需随 iOS 大版本重新编译适配，是持续性维护成本而非一次性移植；
- **性能天花板**：USB 链路上截屏往返 200-400ms/帧，仅支撑 1-3 fps 的找色找图节奏，
  不承诺高性能场景；
- **明确的边界**：不做越狱通道（合规红线）、不做 iOS 端侧 Agent（技术上不可行）、
  不做 iOS Dashboard 原生客户端（Dashboard 走 Web，浏览器即可）。

启动实施的条件（真实需求 + Mac 签名环境 + A 线交付完毕）写在上引设计文档 §7，
本文不展开。

## CI 验证对应

支持矩阵的每个承诺，都要能回答"哪个 CI job 在验证它"。下表是当前 `.github/workflows/`
的实际覆盖（[ci.yml](../.github/workflows/ci.yml) 与打包 workflow）：

| 平台 | job | 运行环境 | 验证深度 | 打包验证 |
|------|-----|----------|----------|----------|
| Windows | `Platform Boundary Guard` | ubuntu-latest | 全平台平台宏边界检查（每提交） | — |
| Windows | `C++ Windows` | windows-2022 | **C++ 全量测试 + 覆盖率**（OpenCppCoverage） | ✅（`build-package.yml`，zip） |
| Windows | `C++ Windows (Python engine)` | windows-2022 | Python 引擎实验编译 + best-effort 测试 | — |
| Linux | `C++ Linux (full tests)` | ubuntu-24.04 | **C++ 完整构建 + 全量核心测试**（xvfb-run 提供虚拟显示，openbox/xclip 随 apt 安装） | — |
| Linux | `C++ ubuntu-22.04`（compat matrix） | ubuntu-22.04 | ⚠️ **仅编译** compat 层 `wingman_transport` 单 target，**无核心测试** | ✅（tar.gz/AppImage/deb） |
| macOS | `C++ macos-15-intel`（compat matrix） | macos-15-intel | ⚠️ 同上，且 `continue-on-error: true`（失败不阻塞合并） | ✅（tar.gz/dmg/app） |
| Android | — | — | ❌ **无 CI 测试 job** | ✅（debug 签名 APK） |
| Go Server | `Go Server`（3 OS matrix） | ubuntu/windows/macos-latest | `go vet` + `go test -race`（全平台） | — |

**必须如实标注的现状**（即"为什么这么组织"一节所说的对账结论）：

1. **Linux 的 C++ 核心测试已由 `C++ Linux (full tests)` job 覆盖**：ubuntu-24.04 上
   完整构建 + 全量核心测试，X11 用例经 xvfb-run 在虚拟显示下真实执行，非 skip。
   但需注意该 job 使用的 GCC 版本（13）与 compat matrix（ubuntu-22.04 的 GCC 11）
   不同。最低支持工具链 GCC 11 的全量编译已于 2026-09-25 本地实测（g++-11 完整
   构建，vcpkg 依赖全重编）：**唯一缺口**是 `ScriptValue::objectVal`——类内递归
   `std::unordered_map<std::string, ScriptValue>` 成员（标准仅给 vector/list/
   forward_list 不完整类型豁免；GCC 15 的 libstdc++ 碰巧容忍，GCC 11 在 pair
   实例化时正确拒绝），连锁导致脚本子系统 53 个编译目标失败，其余目标全部编译
   通过。修复需将对象表示改为标准豁免容器或间接层（`fromObject` 243 处调用 +
   `objectVal` 35 处访问的破坏性重构），暂不进行——GCC 11 支持维持在 transport
   层（compat job）的既有承诺范围。
2. **macOS 的 C++ 核心仍无测试覆盖，且 job 允许失败**（continue-on-error），意味着
   macOS 的"已支持"目前由打包成功 + 兼容层编译成功支撑，核心行为无 CI 证据。
3. **Android 只有打包验证**。签名/发布流程的工程化在 A3 里程碑中，测试 job 亦然。
4. **iOS 无任何 CI**（对应"规划中"状态）。

补齐方向与优先级不在本文展开（那是 CI 演进决策，不是平台现状记录），但缺口本身
必须在这里可见。

## 本机验证命令（Linux 开发环境）

Linux 是当前主力开发平台，以下命令在本仓库根目录可直接使用（其他平台的构建见
[BUILD](../BUILD.md)，Windows 命令亦在该文档）：

```bash
# 配置（Debug + 测试；Linux 测试需要 tests feature 与 WINGMAN_BUILD_TESTS）
cmake -B build-cov \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg根>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DWINGMAN_BUILD_TESTS=ON \
  -DBUILD_CORE_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES=tests

# 编译
cmake --build build-cov --target core_tests -j"$(nproc)"

# 运行全量测试（需要 X server；无头环境先启动：Xvfb :98 & export DISPLAY=:98）
./build-cov/lib/wingman/tests/core_tests
```

两个已踩过的注意点：

1. **测试要求数 PASSED 计数**，而非只看退出码——超时被杀的测试进程也可能以
   `[  PASSED  ]` 之前的中间状态留下看似正常的输出，核对 `[==========] N tests ran`
   与 PASSED 数是否吻合；
2. **GUI/屏幕类用例在无显示环境会 skip**（如剪贴板图像、窗口操作类），这是设计行为，
   全量 PASSED + 若干 SKIPPED 是正常形态。

## 跨平台统一 API 收敛路线

现状的差异（上表所有 ⚠️/❌）可以归为三类，收敛策略也相应分三条，本文只记录方向不画饼：

1. **系统能力缺失类**（Linux 无 UI Automation、macOS 无 OCR/ML 依赖）——不强行补齐，
   以脚本侧能力探测 + 明确的降级返回值为准；是否引入新依赖由 vcpkg manifest 的平台
   覆盖决策，不走系统库探测。
2. **架构性分层类**（Android 不加载桌面模块注册表）——收敛方向是"同名同形"：A 线
   子表已与桌面 API 同名同形（`wingman.input.*` 等），后续按需求逐面扩大子表覆盖，
   而不是把桌面注册表整体搬上端侧。
3. **多后端抽象类**（iOS 主机控路线）——不新增平台分支，而是给现有
   `ICapture`/`IInput` 增加设备后端（"设备即一种屏幕"），脚本语义与桌面完全一致。
   这是平台收敛的终态示范：脚本不变，后端可插拔。

任何一步的落地都会先出现在 [平台支持矩阵](#平台支持矩阵) 与 [API 适用性标注](#api-适用性标注)
的对应行更新中——本文随代码同 commit 修订。
