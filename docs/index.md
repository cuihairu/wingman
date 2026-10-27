---
layout: home

hero:
  name: "Wingman"
  text: "Programmable Control Engine for Game Automation"
  tagline: "C++ core, with Lua and Python scripting"
  actions:
    - theme: brand
      text: Get Started
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
    title: High Performance
    details: C++ core engine, Lua/Python script execution, millisecond-level response
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><rect x="4.5" y="10.5" width="15" height="10" rx="2"/><path d="M8 10.5V7.5a4 4 0 0 1 8 0v3"/><path d="M12 14.5v2.5"/></svg>'
    title: Safe and Reliable
    details: Runs purely in user mode, uses legitimate platform APIs, does not read or write game memory
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="m8.5 7-4.5 5 4.5 5"/><path d="m15.5 7 4.5 5-4.5 5"/><path d="m13.2 5.5-2.4 13"/></svg>'
    title: Multi-language Support
    details: Supports both Lua and Python as scripting languages, choose freely
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><circle cx="12" cy="12" r="8.5"/><path d="M3.5 12h17"/><path d="M12 3.5a13 13 0 0 1 0 17"/><path d="M12 3.5a13 13 0 0 0 0 17"/></svg>'
    title: Cross-platform
    details: Supports Windows, macOS and Linux behind a unified interface abstraction
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><circle cx="12" cy="13.5" r="4"/><path d="M12 9.5V6.5"/><path d="m10.4 4.9 1.6 1.6 1.6-1.6"/><path d="M8 13.5H4.5"/><path d="M20 13.5h-3.5"/><path d="m8.4 10.2-2.2-2.2"/><path d="m15.6 10.2 2.2-2.2"/><path d="m8.4 16.8-2.2 2.2"/><path d="m15.6 16.8 2.2 2.2"/></svg>'
    title: Debugging Support
    details: VS Code Lua hints extension; breakpoint debugging planned (script debugger is a stub placeholder)
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><path d="M3.5 18.5C7 11 11 20.5 20.5 6"/><path d="m17.5 5.5 3.4.6-.7 3.4"/><circle cx="6" cy="7" r="2"/><path d="M6 9v3.5"/></svg>'
    title: Human-like Input
    details: Bézier-curve mouse movement, random delays and a natural-operation mode
  - icon: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" width="24" height="24"><rect x="6" y="2.5" width="12" height="19" rx="2.5"/><path d="M10 6.5h4"/><path d="M10.5 17.5h3"/></svg>'
    title: Android Agent
    details: Gesture injection, screen capture, color and image search, remote screenshots, with the APK distributed alongside nightly

---
> [中文](zh/index.md) | **English**

## Introduction

**Wingman** is a cross-platform game automation tool.

- Core engine built on **C++**, with screen capture and input injection implemented in the native layer
- Supports two script engines, **Lua** and **Python**, with one shared API
- Runs purely in **user mode**, using legitimate platform APIs, safe and reliable
- Supports **remote orchestration**: the runtime agent connects out to the Go server, which acts as the unified central controller
- Supports a **local standalone UI**: Tauri controls the runtime over local IPC, not through a runtime WebSocket/HTTP server

## Core Features

- **Screen operations** - screenshots, pixel detection, color matching, image search
- **Input simulation** - mouse click/move, key sending, text input
- **Window management** - find window, activate window, get position
- **UI Automation** - manipulate Windows controls directly, no coordinate targeting needed
- **Process management** - start/wait/terminate processes
- **Macro recording** - record mouse and keyboard operations, replay automatically
- **Trigger system** - pixel triggers, timed triggers, conditional combinations
- **Orchestration layer** - the runtime agent connects out to the Go server, and the Dashboard connects only to the Go server
- **Debugger** - breakpoint debugging planned (stub placeholder for now); VS Code Lua hints extension available
- **Human-like input** - Bézier curves, random delays, natural operation
- **Storage system** - a four-layer storage architecture supporting local and remote data persistence
- **Version management** - dynamic version information with support for nightly builds

## Quick Start

### Lua Scripting

```lua
local wingman = require("wingman")

-- hello.lua
-- 截图（capture 返回布尔：是否成功）
local ok = wingman.screen.capture()

-- 查找颜色（返回 {point, found} 两元组；区域用表：{x=, y=, width=, height=}）
local pt, found = wingman.screen.findColor(0xFF0000, {x = 0, y = 0, width = 1920, height = 1080}, 10)
if found then
    wingman.input.click(pt.x, pt.y)
end
-- 多点查找用 findColors（返回点数组）：wingman.screen.findColors(0xFF0000, region, 10, 0)
```

### Python Scripting

```python
# hello.py
from wingman import screen, input

# 截图（capture 返回布尔：是否成功）
ok = screen.capture()

# 查找颜色（返回 [point, found] 两元组；区域用字典）
pt, found = screen.findColor(0xFF0000, {"x": 0, "y": 0, "width": 1920, "height": 1080}, 10)
if found:
    input.click(pt["x"], pt["y"])
# 多点查找用 find_colors（返回点数组）：screen.find_colors(0xFF0000, region, 10, 0)
```

## Android Agent

The Android on-device Agent (experimental, arm64 / Android 9.0+): it dials out over TCP to the Go central controller, and its script APIs carry the same names and shapes as the desktop runtime (`wingman.input` / `wingman.screen` / `wingman.vision`), with the Dashboard handling delivery and inspection uniformly.

**Capability list**

| Capability | Description |
|------|------|
| Gesture injection | The accessibility service injects taps and swipes (`wingman.input.tap` / `swipe` / `delay`) |
| Screen capture | Real-time screen-projection capture via MediaProjection (frames are produced while the screen is on) |
| Color / image search | `wingman.vision.findColor` / `findImage` (on-device uses the pure C++ PatternMatcher; the desktop API of the same name goes through OpenCV, and the two paths use different score conventions) |
| Remote screenshots | The screenshot step of a Dashboard workflow puts the image on screen directly |
| token authentication | A registration-token allowlist via `WINGMAN_AGENT_TOKENS` on the server side (off by default, may be left empty) |
| Keep-alive | Boot auto-start, crash self-restart (exponential backoff), a 30s core watchdog, plus per-model keep-alive and restricted-settings guidance |

**Getting the APK**

From the nightly Release, take `wingman-*-android-arm64.apk` (debug signed):
[releases/tag/nightly](https://github.com/cuihairu/wingman/releases/tag/nightly)
— from that page's Assets, pick the newest `wingman-<date>-nightly-<sha>-android-arm64.apk`
(daily builds keep only the latest of the day; direct anonymous download, no GitHub login required).

**Getting started**

1. Start the Go server (`orchestrator/server`, Dashboard port 9527, agent port 8888).
2. Install the APK on the phone (same subnet as the server) → open the App → fill in the server IP / port / device ID
   → tap "Start Agent". The first launch creates a persistent notification (required by the foreground service); if the server has `WINGMAN_AGENT_TOKENS` configured, enter the registration Token as well.
3. The device appears in the Dashboard's Agent list (platform=android); create a new script
   `print('hello android')` and run it against that device, with live logs streaming back.
4. Enable the A2 capabilities: turn on the injection service under "Accessibility Settings" in the App and grant "Enable screen projection"
   (on Android 14+ every restart of screen projection pops the authorization dialog again, a system constraint), then deliver a color-search / gesture script:

   ```lua
   local pt = wingman.vision.findColor(0xE23B3B, 10)
   if pt then wingman.input.tap(pt.x, pt.y) end
   wingman.input.swipe(540, 1800, 540, 600, 400)
   ```

5. Remote screenshot: add a screenshot step to a Dashboard workflow and select that device → the image is put on screen.
6. On Android 13+, accessibility for sideloaded apps is blocked by default under "restricted settings": system Settings → Apps →
   Wingman Agent → ⋮ → Allow restricted settings, or pre-authorize with
   `scripts/android-restricted-settings.sh allow` (there is in-app guidance too).

For detailed acceptance steps and known boundaries see
[apps/android/README.md](https://github.com/cuihairu/wingman/blob/main/apps/android/README.md),
for the design and protocol see [Android Agent Design](./zh/android-agent-design.md),
for per-model keep-alive see [Android Agent Keep-alive](/guides/android-keep-alive),
and for restricted settings see [Android Agent Restricted Settings](/guides/android-restricted-settings).

## Building the Project

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

## Running an Example

```bash
# CLI 运行时（Python 脚本同理，需启用 WINGMAN_ENABLE_PYTHON）
./build/apps/agent/wingman-agent script examples/lua_scripts/hello.lua
```

## System Requirements

### Windows
- Windows 10/11 (x64)
- Visual Studio 2022

### macOS
- macOS 12+ (Monterey or newer)
- Xcode 14+ or Clang

### Linux
- Ubuntu 22.04+ or an equivalent distribution
- GCC 11+ or Clang 14+

### General
- CMake 3.20+
- vcpkg

## License

[Apache-2.0 License](https://github.com/cuihairu/wingman/blob/main/LICENSE)