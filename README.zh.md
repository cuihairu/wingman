<div align="center">

<img src="docs/public/logo.svg" alt="Wingman" width="100" />

# Wingman

[![OS](https://img.shields.io/badge/OS-Windows%20%7C%20macOS%20%7C%20Linux-blue.svg)](https://github.com/cuihairu/wingman)
[![CI](https://github.com/cuihairu/wingman/workflows/CI/badge.svg)](https://github.com/cuihairu/wingman/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cuihairu/wingman/branch/main/graph/badge.svg)](https://codecov.io/gh/cuihairu/wingman)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Lua](https://img.shields.io/badge/Lua-5.5-000080.svg?logo=lua&logoColor=white)](https://www.lua.org/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB.svg?logo=python&logoColor=white)](https://www.python.org/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

**跨平台可编程自动化 Agent 与远程编排平台**（游戏自动化是第一垂直场景）

C++ + Lua/Python 的自动化 Agent Runtime + Go 远程编排中控

> 一句话定位：把 Windows / macOS / Linux / Android 机器统一注册成 Agent，通过
> 脚本、视觉、输入、OCR/ML 与 Workflow 对它们做自动化控制；游戏自动化是打磨
> 最深的垂直场景，但控制面与执行面从一开始就是按通用 Agent 平台设计的。

[English](README.md) | 简体中文

[文档](docs/README.md) · [快速开始](docs/guide/getting-started.md) · [平台支持](docs/platforms.md) · [API 参考](docs/api/overview.md) · [示例](docs/examples/) · [贡献指南](CONTRIBUTING.md)

</div>

> [**免责声明**]
>
> 本工具仅供合法场景使用，包括但不限于：自动化测试、可单机游戏辅助、无障碍辅助等。
> 使用本工具违反任何游戏或软件的用户协议所导致的后果，由使用者自行承担。
> 作者不对因使用本工具而产生的任何法律责任负责。

---

## 演示站点

**演示站点 https://wingman.cuihairu.site/ ｜ 演示账号 `demo` / `Wing-demo-Isx4Dvm7eSd6-26!`**（体验用，数据定期重置）

> 演示站点由 CI 自动部署与回滚（main 分支测试全绿才发布），数据会被定期重置，仅供体验；演示账号为只读 `viewer` 角色。

---

## 核心特性

- **C++ 核心** - 引擎与脚本模块以 C++23 实现，Lua/Python 只做脚本层
- **双语言脚本** - Lua（sol2）与 Python（pybind11）共用同一套 `wingman.*` API
- **纯用户态** - 只调用系统公开 API，不读写游戏内存、不注入进程
- **可编程** - 41 个脚本模块加触发器、宏，复杂逻辑写成脚本
- **跨平台** - Windows、macOS、Linux 统一接口抽象；Android 实验性支持、iOS 规划中（[平台支持详情](docs/platforms.md)）

### 功能模块

| 模块 | 功能 |
|------|------|
| **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 |
| **输入模拟** | 鼠标点击/移动、按键发送、文本输入 |
| **窗口管理** | 查找窗口、激活窗口、获取位置 |
| **触发器系统** | 像素/图像/时间条件触发，自动执行动作 |
| **宏录制** | 录制鼠标键盘操作，保存为脚本回放 |
| **UI Automation** | Windows UIA 自动化，操作 UI 控件 |
| **OCR 识别** | Tesseract 文字识别（可选依赖） |
| **数据持久化** | kv 键值存储、SQLite 数据库 |
| **序列化格式** | JSON、INI 配置文件解析 |
| **文件 IO** | 文件读写（二进制安全）、移动复制、目录操作（`wingman.file`） |
| **全局热键** | 轮询式全局键态监听、组合键回调（`wingman.hotkey`，Python 回调） |
| **通知系统** | 日志/Toast/Webhook/事件桥接、脚本托盘意图（`wingman.notify`） |
| **调试支持** | VS Code 断点调试 Lua 脚本（需启用调试组件） |
| **Android Agent** | Android 端侧 Agent（实验性，arm64 / Android 9.0+），出站 TCP 连 Go 中控，脚本 API 与桌面 runtime 同名同形，Dashboard 统一下发与查看 |
| **远程编排** | Agent 注册/心跳/命令分发（16 字节头 + JSON 帧、单命令超时）、批量操作（按 ID/标签选择器 fan-out）、DAG 工作流、RBAC/审计、Guacamole 远程桌面（Go 中控） |

> 部分高级模块（OCR、ML/YOLO、远程编排、脚本调试器）依赖可选组件或仍处于持续建设中。默认可用能力以当前构建配置、运行时参数和对应 API 文档为准。

---

## 快速开始

### 一键安装 Agent

一条命令安装 `wingman-agent` 单二进制（自动检测 OS 与 CPU 架构，匿名下载，无需登录 GitHub；重跑即升级）：

**Linux / macOS:**
```bash
curl -fsSL https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.sh | bash
```

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.ps1 | iex
```

可选参数（Linux/macOS 用 `bash -s --`，Windows 用 scriptblock）：

```bash
# 指定版本 + 注册常驻服务（Linux: systemd user unit；macOS: launchd）
curl -fsSL https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.sh | bash -s -- --version nightly --service
```

```powershell
# 指定版本 + 注册 Windows 服务（需管理员；服务运行于 Session 0，仅适合 headless）
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.ps1))) -Version nightly -Service
```

| OS | 架构 | 产物 |
|----|------|------|
| Linux | x86_64 (x64) / aarch64 (arm64) | `wingman-agent-*-linux-{x64,arm64}.tar.gz` |
| macOS | x86_64 (x64) / arm64 (Apple Silicon) | `wingman-agent-*-macos-{x64,arm64}.tar.gz` |
| Windows | x64 / arm64 | `wingman-agent-*-windows-{x64,arm64}.zip` |

说明：

- 默认安装到 `~/.local/bin`（Linux/macOS）或 `%LOCALAPPDATA%\Programs\Wingman\bin`（Windows），安装后自动运行 `wingman-agent --version` 验证
- 版本选择：按发布时间倒序取第一个含本平台 agent 产物的 release（正式版优先，尚无正式 agent 产物时落到 nightly 预发布）
- 下载走 GitHub Releases 资产匿名直链；API 查询有 60 次/小时匿名限流，可用 `--token`/`-Token`（或 `GITHUB_TOKEN` 环境变量）提升额度
- 不认识的 OS/架构会明确报错并列出支持范围（armv7 等暂无构建产物）
- 完整参数见 [scripts/install.sh](scripts/install.sh) / [scripts/install.ps1](scripts/install.ps1) 头部注释

### 环境要求

- **Windows**: Windows 10/11 + Visual Studio 2022
- **macOS**: macOS 12+ + Xcode 14+
- **Linux**: Ubuntu 22.04+ + GCC 11+
- **通用**: CMake 3.20+ + vcpkg

### 安装 vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### 编译运行

**Windows:**
```bash
# 编译
build-scripts\build-runtime-msvc-ninja.bat

# 运行 Lua 脚本
.\build-msvc-ninja-vcpkg\apps\agent\wingman-agent.exe script examples\hello.lua
```

**详细构建步骤请查看 [构建指南](BUILD.md)**

---

## Android Agent

Android 端侧 Agent（实验性，arm64 / Android 9.0+）：出站 TCP 连 Go 中控，
脚本 API 与桌面 runtime 同名同形（`wingman.input` / `wingman.screen` /
`wingman.vision`），Dashboard 统一下发与查看。

**能力清单**

| 能力 | 说明 |
|------|------|
| **手势注入** | 无障碍服务注入点击/滑动（`wingman.input.tap` / `swipe` / `delay`） |
| **屏幕采集** | MediaProjection 实时投屏采集（亮屏时出帧） |
| **找色找图** | `wingman.vision.findColor` / `findImage`（OpenCV 模板匹配，进 NDK） |
| **远程截图** | Dashboard workflow 的 screenshot 步骤直接上屏（与桌面同形） |
| **token 认证** | server 端 `WINGMAN_AGENT_TOKENS` 注册 token 白名单（默认关闭可留空） |
| **保活** | 开机自启、崩溃自重启（指数退避）、30s 核心看门狗、机型保活与受限设置引导 |

**获取 APK**

nightly Release 的 `wingman-*-android-arm64.apk`（debug 签名）：

- 发布页：[releases/tag/nightly](https://github.com/cuihairu/wingman/releases/tag/nightly)
- Assets 里取最新一组的 `wingman-<date>-nightly-<sha>-android-arm64.apk`
  （nightly 每日构建，仅保留当日最新）
- debug 签名：手机安装时需允许「未知来源」；无需登录 GitHub，直链匿名下载

**快速上手**

1. 启动 Go server（`orchestrator/server`，Dashboard 端口 9527、agent 端口 8888）。
2. 手机安装 APK（与 server 同网段），打开 App → 填服务器 IP / 端口 / 设备 ID
   → 「启动 Agent」。首次启动会创建常驻通知（前台服务要求）；server 若配置了
   `WINGMAN_AGENT_TOKENS`（注册 token 白名单），需同时填入注册 Token。
3. Dashboard 的 Agent 列表出现该设备（platform=android）。
4. 新建脚本运行到该设备，实时日志回传：

   ```lua
   print('hello android')
   ```

5. 启用 A2 能力：App 内「无障碍设置」打开注入服务 +「开启投屏」授权
   （Android 14+ 每次重开投屏都会再弹授权，系统约束），随后下发找色/手势脚本：

   ```lua
   local pt = wingman.vision.findColor(0xE23B3B, 10)
   if pt then wingman.input.tap(pt.x, pt.y) end
   wingman.input.swipe(540, 1800, 540, 600, 400)
   ```

6. 远程截图：Dashboard workflow 加 screenshot 步骤选该设备 → 截图上屏。
7. Android 13+ 侧载安装的无障碍被「受限设置」默认屏蔽：系统设置 → 应用 →
   Wingman Agent → ⋮ → 允许受限制的设置，或电脑连真机执行
   `scripts/android-restricted-settings.sh allow` 预授权（App 内也有引导）。

详细验收步骤、已知边界与 A3 可靠性验证见 [apps/android/README.md](apps/android/README.md)，
设计与协议见 [docs/android-agent-design.md](docs/android-agent-design.md)，
机型保活见 [docs/guides/android-keep-alive.md](docs/guides/android-keep-alive.md)。

---

## 代码示例

### Lua

```lua
local wingman = require("wingman")

-- 截图并保存
local screenshot = wingman.screen.capture(0, 0, 1920, 1080)
screenshot:save("screenshot.png")

-- 查找颜色并点击
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, p in ipairs(points) do
        wingman.input.click(p.x, p.y, "left")
    end
end
```

### Python

```python
from wingman import screen, input

# 截图
screenshot = screen.capture(0, 0, 1920, 1080)
screenshot.save("screenshot.png")

# 查找颜色并点击
points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points:
    for p in points:
        input.click(p["x"], p["y"])
```

**更多示例请查看 [示例文档](docs/examples/)**

---

## 文档

- [快速开始](docs/guide/getting-started.md) - 5 分钟上手指南
- [构建指南](BUILD.md) - 详细的构建、测试和故障排除说明
- [平台支持](docs/platforms.md) - 各平台状态、API 适用性与环境差异
- [API 参考](docs/api/overview.md) - 完整的 API 文档
- [架构设计](docs/architecture.md) - 系统架构和设计决策
- [开发指南](docs/DEVELOPMENT.md) - 贡献和开发指南
- [VS Code 开发环境](docs/development-environment.md) - Lua/Python 补全、任务和调试配置

---

## 架构概览

Wingman 采用 **C++ 核心引擎 + 多语言脚本** 的架构设计：

- **控制面**: Go server 作为远程中控，runtime 作为 agent 主动连接
- **本地控制**: Tauri GUI 通过本地 IPC 控制 runtime
- **脚本引擎**: Lua (sol2) 和 Python (pybind11) 统一接口
- **模块化**: 41 个语言无关脚本模块（见 [API 文档](docs/api/index.md)），易于扩展

详细架构说明请查看 [架构文档](docs/architecture.md)

---

## 技术底座

本项目基于开源组件构建，主要来源：

- 脚本引擎：Lua 5.5（vcpkg `lua` 端口，sol2 绑定）、Python 3（pybind11 嵌入）
- C++ 基础库：asio、cURL、OpenSSL、nlohmann-json、spdlog、SQLite，统一由 vcpkg manifest 管理
- 视觉与识别：OpenCV（模板匹配）、Tesseract（OCR）、ONNX Runtime（ML 推理），均为可选依赖
- 桌面 GUI：Tauri + Svelte
- 控制面板：Dashboard 基于 React + Umi，服务端为 Go
- 远程桌面网关：基于 Apache Guacamole（guacd）
- 构建体系：CMake + vcpkg

---

## 贡献

贡献方式见 [开发指南](docs/DEVELOPMENT.md)。

---

## 许可证

[Apache-2.0](LICENSE)

---

<div align="center">

**[返回顶部](#wingman)**

</div>
