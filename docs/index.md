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
  - title: 🚀 高性能
    details: C++ 核心引擎，Lua/Python 脚本执行，毫秒级响应
  - title: 🔒 安全可靠
    details: 纯用户态运行，使用合法平台 API，不读写游戏内存
  - title: 🎮 多语言支持
    details: 支持 Lua 和 Python 两种脚本语言，灵活选择
  - title: 🌐 跨平台
    details: 支持 Windows、macOS、Linux，统一接口抽象
  - title: 🐛 强大的调试
    details: VS Code 插件支持，断点调试、变量查看、性能分析
  - title: 🤖 人性化模拟
    details: 贝塞尔曲线鼠标移动、随机延迟、自然操作模式

---

## 简介

**Wingman** 是一个跨平台的游戏自动化工具。

- 基于 **C++** 开发核心引擎，提供高性能的屏幕操作和输入模拟能力
- 支持 **Lua** 和 **Python** 两种脚本引擎，灵活可扩展
- 纯**用户态**运行，使用合法平台 API，安全可靠
- 支持**远程编排**，runtime agent 主动连接 Go server，由 Go server 统一中控
- 支持**本地单机 UI**，Tauri 通过本地 IPC 控制 runtime，不通过 runtime WebSocket/HTTP server

## 核心特性

- 📷 **屏幕操作** - 截图、像素检测、颜色匹配、图像查找
- 🖱️ **输入模拟** - 鼠标点击/移动、按键发送、文本输入
- 🪟 **窗口管理** - 查找窗口、激活窗口、获取位置
- 🤖 **UI Automation** - 直接操作 Windows 控件，无需坐标定位
- ⚙️ **进程管理** - 启动/等待/终止进程
- 🔄 **宏录制** - 录制鼠标键盘操作，自动回放
- 🎯 **触发器系统** - 像素触发、定时触发、条件组合
- 🌐 **编排层** - Runtime agent 主动连接 Go server，Dashboard 只连接 Go server
- 🐛 **调试器** - VS Code 插件，断点调试、变量查看
- 🤖 **人性化模拟** - 贝塞尔曲线、随机延迟、自然操作
- 💾 **存储系统** - 四层存储架构，支持本地和远程数据持久化
- 🏷️ **版本管理** - 动态版本信息，支持 nightly 构建

## 快速开始

### Lua 脚本

```lua
local wingman = require("wingman")

-- hello.lua
-- 截图
local img = wingman.screen.capture(0, 0, 1920, 1080)

-- 查找颜色（Lua 使用 camelCase 命名）
local x, y = wingman.screen.findPixel(0xFF0000, 0, 0, 1920, 1080, 10)
if x then
    wingman.input.click(x, y)
end
```

### Python 脚本

```python
# hello.py
from wingman import screen, input

# 截图
img = screen.capture(0, 0, 1920, 1080)

# 查找颜色（Python 使用 snake_case 命名）
result = screen.find_pixel(0xFF0000, 0, 0, 1920, 1080, 10)
if result:
    x, y = result
    input.click(x, y)
```

## 编译项目

```bash
# 克隆仓库
git clone https://github.com/cuihairu/wingman.git
cd wingman

# Windows (MSVC + Ninja + vcpkg)
build-scripts\build-runtime-msvc-ninja.bat

# macOS/Linux (GCC/Clang)
cmake -B build -S . -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux

```

## 运行示例

```bash
# CLI 运行时
./build/apps/runtime/Release/wingman-runtime script examples/hello.lua

# 或 Python 脚本（需要启用 WINGMAN_ENABLE_PYTHON）
./build/apps/runtime/Release/wingman-runtime script examples/hello.py
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
