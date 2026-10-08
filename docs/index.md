---
layout: home

hero:
  name: "Wingman"
  text: "游戏自动化可编程控制引擎"
  tagline: "C++ 核心，支持 Lua 和 Python 脚本"
  actions:
    - theme: brand
      text: 快速开始
      link: /guide/getting-started
    - theme: alt
      text: GitHub
      link: https://github.com/cuihairu/wingman
  image:
    src: /logo.svg
    alt: Wingman
    width: 120

features:
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="M13 2.5 4.5 14h6.5l-1 7.5L18.5 10h-6.5z"/></svg>'
    title: 高性能
    details: C++ 核心引擎，Lua/Python 脚本执行，毫秒级响应
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><rect x="4.5" y="10.5" width="15" height="10" rx="2"/><path d="M8 10.5V7.5a4 4 0 0 1 8 0v3"/><path d="M12 14.5v2.5"/></svg>'
    title: 安全可靠
    details: 纯用户态运行，使用合法平台 API，不读写游戏内存
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="m8.5 7-4.5 5 4.5 5"/><path d="m15.5 7 4.5 5-4.5 5"/><path d="m13.2 5.5-2.4 13"/></svg>'
    title: 多语言支持
    details: 支持 Lua 和 Python 两种脚本语言，灵活选择
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><circle cx="12" cy="12" r="8.5"/><path d="M3.5 12h17"/><path d="M12 3.5a13 13 0 0 1 0 17"/><path d="M12 3.5a13 13 0 0 0 0 17"/></svg>'
    title: 跨平台
    details: 支持 Windows、macOS、Linux，统一接口抽象
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><circle cx="12" cy="13.5" r="4"/><path d="M12 9.5V6.5"/><path d="m10.4 4.9 1.6 1.6 1.6-1.6"/><path d="M8 13.5H4.5"/><path d="M20 13.5h-3.5"/><path d="m8.4 10.2-2.2-2.2"/><path d="m15.6 10.2 2.2-2.2"/><path d="m8.4 16.8-2.2 2.2"/><path d="m15.6 16.8 2.2 2.2"/></svg>'
    title: 调试支持
    details: VS Code 插件支持，断点调试、变量查看、性能分析
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="M3.5 18.5C7 11 11 20.5 20.5 6"/><path d="m17.5 5.5 3.4.6-.7 3.4"/><circle cx="6" cy="7" r="2"/><path d="M6 9v3.5"/></svg>'
    title: 拟人化输入
    details: 贝塞尔曲线鼠标移动、随机延迟、自然操作模式
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><rect x="6" y="2.5" width="12" height="19" rx="2.5"/><path d="M10 6.5h4"/><path d="M10.5 17.5h3"/></svg>'
    title: Android Agent
    details: 手势注入、屏幕采集、找色找图、远程截图，APK 随 nightly 分发

---

## 简介

**Wingman** 是一个跨平台的游戏自动化工具。

- 基于 **C++** 开发核心引擎，屏幕捕获与输入注入在原生层实现
- 支持 **Lua** 和 **Python** 两种脚本引擎，同一套 API
- 纯**用户态**运行，使用合法平台 API，安全可靠
- 支持**远程编排**，runtime agent 主动连接 Go server，由 Go server 统一中控
- 支持**本地单机 UI**，Tauri 通过本地 IPC 控制 runtime，不通过 runtime WebSocket/HTTP server

## 核心特性

- **屏幕操作** - 截图、像素检测、颜色匹配、图像查找
- **输入模拟** - 鼠标点击/移动、按键发送、文本输入
- **窗口管理** - 查找窗口、激活窗口、获取位置
- **UI Automation** - 直接操作 Windows 控件，无需坐标定位
- **进程管理** - 启动/等待/终止进程
- **宏录制** - 录制鼠标键盘操作，自动回放
- **触发器系统** - 像素触发、定时触发、条件组合
- **编排层** - Runtime agent 主动连接 Go server，Dashboard 只连接 Go server
- **调试器** - VS Code 插件，断点调试、变量查看
- **拟人化输入** - 贝塞尔曲线、随机延迟、自然操作
- **存储系统** - 四层存储架构，支持本地和远程数据持久化
- **版本管理** - 动态版本信息，支持 nightly 构建

## 快速开始

### Lua 脚本

```lua
local wingman = require("wingman")

-- hello.lua
-- 截图
local img = wingman.screen.capture(0, 0, 1920, 1080)

-- 查找颜色（Lua 使用 camelCase 命名）
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, p in ipairs(points) do
        wingman.input.click(p.x, p.y)
    end
end
```

### Python 脚本

```python
# hello.py
from wingman import screen, input

# 截图
img = screen.capture(0, 0, 1920, 1080)

# 查找颜色（API 名与 Lua 一致，camelCase）
points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points:
    for p in points:
        input.click(p["x"], p["y"])
```

## Android Agent

Android 端侧 Agent（实验性，arm64 / Android 9.0+）：出站 TCP 连 Go 中控，
脚本 API 与桌面 runtime 同名同形（`wingman.input` / `wingman.screen` /
`wingman.vision`），Dashboard 统一下发与查看。

**能力清单**

| 能力 | 说明 |
|------|------|
| 手势注入 | 无障碍服务注入点击/滑动（`wingman.input.tap` / `swipe` / `delay`） |
| 屏幕采集 | MediaProjection 实时投屏采集（亮屏时出帧） |
| 找色找图 | `wingman.vision.findColor` / `findImage`（OpenCV 模板匹配） |
| 远程截图 | Dashboard workflow 的 screenshot 步骤直接上屏 |
| token 认证 | server 端 `WINGMAN_AGENT_TOKENS` 注册 token 白名单（默认关闭可留空） |
| 保活 | 开机自启、崩溃自重启（指数退避）、30s 核心看门狗、机型保活与受限设置引导 |

**获取 APK**

nightly Release 的 `wingman-*-android-arm64.apk`（debug 签名）：
[releases/tag/nightly](https://github.com/cuihairu/wingman/releases/tag/nightly)
页 Assets 取最新一组的 `wingman-<date>-nightly-<sha>-android-arm64.apk`
（每日构建仅保留当日最新；直链匿名下载，无需登录 GitHub）。

**快速上手**

1. 启动 Go server（`orchestrator/server`，Dashboard 端口 9527、agent 端口 8888）。
2. 手机安装 APK（与 server 同网段）→ 打开 App → 填服务器 IP / 端口 / 设备 ID
   → 「启动 Agent」。首次启动创建常驻通知（前台服务要求）；server 若配置了
   `WINGMAN_AGENT_TOKENS`，需同时填入注册 Token。
3. Dashboard 的 Agent 列表出现该设备（platform=android），新建脚本
   `print('hello android')` 运行到该设备，实时日志回传。
4. 启用 A2 能力：App 内「无障碍设置」打开注入服务 +「开启投屏」授权
   （Android 14+ 每次重开投屏都会再弹授权，系统约束），随后下发找色/手势脚本：

   ```lua
   local pt = wingman.vision.findColor(0xE23B3B, 10)
   if pt then wingman.input.tap(pt.x, pt.y) end
   wingman.input.swipe(540, 1800, 540, 600, 400)
   ```

5. 远程截图：Dashboard workflow 加 screenshot 步骤选该设备 → 截图上屏。
6. Android 13+ 侧载无障碍被「受限设置」默认屏蔽：系统设置 → 应用 →
   Wingman Agent → ⋮ → 允许受限制的设置，或
   `scripts/android-restricted-settings.sh allow` 预授权（App 内也有引导）。

详细验收步骤与已知边界见
[apps/android/README.md](https://github.com/cuihairu/wingman/blob/main/apps/android/README.md)，
设计与协议见 [Android Agent 设计](./android-agent-design.md)，
机型保活见 [Android Agent 保活](/guides/android-keep-alive)，
受限设置见 [Android Agent 受限设置](/guides/android-restricted-settings)。

## 编译项目

```bash
# 克隆仓库
git clone https://github.com/cuihairu/wingman.git
cd wingman

# Windows (MSVC + Ninja + vcpkg)
build-scripts\build-runtime-msvc-ninja.bat

# Linux (GCC/Clang)
cmake -B build -S . -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux

# macOS (Intel 用 x64-osx，Apple Silicon 用 arm64-osx)
cmake -B build -S . -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-osx

```

## 运行示例

```bash
# CLI 运行时（Python 脚本同理，需启用 WINGMAN_ENABLE_PYTHON）
./build/apps/runtime/wingman-runtime script examples/lua_scripts/hello.lua
```

## 系统要求

### Windows
- Windows 10/11 (x64)
- Visual Studio 2022

### macOS
- macOS 12+ (Monterey 或更高)
- Xcode 14+ 或 Clang

### Linux
- Ubuntu 22.04+ 或等效发行版
- GCC 11+ 或 Clang 14+

### 通用
- CMake 3.20+
- vcpkg

## 许可证

[Apache-2.0 License](https://github.com/cuihairu/wingman/blob/main/LICENSE)
