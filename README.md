# Wingman

<div align="center">

<img src="docs/public/logo.svg" alt="Wingman" width="100" />

**游戏自动化可编程控制引擎**

C++ + Lua/Python 的高性能游戏自动化框架

[![OS](https://img.shields.io/badge/OS-Windows%20%7C%20macOS%20%7C%20Linux-blue.svg)](https://github.com/cuihairu/wingman)
[![CI](https://github.com/cuihairu/wingman/workflows/CI/badge.svg)](https://github.com/cuihairu/wingman/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cuihairu/wingman/branch/main/graph/badge.svg)](https://codecov.io/gh/cuihairu/wingman)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Lua](https://img.shields.io/badge/Lua-5.4-000080.svg?logo=lua&logoColor=white)](https://www.lua.org/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB.svg?logo=python&logoColor=white)](https://www.python.org/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[文档](https://cuihairu.github.io/wingman/) | [快速开始](#快速开始) | [API](https://cuihairu.github.io/wingman/api/) | [示例](https://cuihairu.github.io/wingman/examples/)

</div>

> ⚠️ **免责声明**
>
> 本工具仅供合法场景使用，包括但不限于：自动化测试、可单机游戏辅助、无障碍辅助等。
> 使用本工具违反任何游戏或软件的用户协议所导致的后果，由使用者自行承担。
> 作者不对因使用本工具而产生的任何法律责任负责。

---

## 简介

**Wingman** 是一个跨平台的游戏自动化工具，采用 C++ 核心引擎 + 多语言脚本（Lua / Python）的架构设计。

- 🚀 **高性能** - C++ 核心引擎，Lua/Python 脚本执行，毫秒级响应
- 🐍 **多语言** - 同时支持 Lua (sol2) 和 Python (pybind11)，统一 API 接口
- 🔒 **安全可靠** - 纯用户态运行，使用合法平台 API，不读写游戏内存
- 🎮 **可编程** - 脚本控制，灵活扩展，支持复杂业务逻辑
- 🌐 **跨平台** - 支持 Windows、macOS、Linux，统一接口抽象

---

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│            Lua 脚本 (.lua) / Python 脚本 (.py)            │
│  (用户编写的自动化逻辑：触发器、宏、图像识别等)           │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│           IScriptEngine (语言无关脚本抽象)                │
│  ScriptEngineFactory → 自动检测语言，创建对应引擎        │
└────────┬───────────────────────────────┬────────────────┘
         │                               │
┌────────▼────────┐            ┌─────────▼────────┐
│  LuaScriptEngine │            │PythonScriptEngine │
│   (sol2 绑定)    │            │ (pybind11+CPython) │
└────────┬────────┘            └─────────┬────────┘
         │                               │
         └───────────┬───────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│       25+ ModuleDescriptor (语言无关模块定义)             │
│   screen | input | window | process | trigger | http ... │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│          C++ 核心库 (lib/wingman)                        │
│  ┌──────────┬──────────┬──────────┬──────────┐          │
│  │  Screen  │  Input   │  Window  │  Vision  │          │
│  │  屏幕操作 │  输入模拟 │  窗口管理 │  视觉识别 │          │
│  └──────────┴──────────┴──────────┴──────────┘          │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              平台抽象层 (platform/)                      │
│   IScreen │ IInput │ IWindow │ ICapture │ IUIAutomation  │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│  Windows API    │    macOS API     │    Linux API       │
│  GDI+/DXGI      │ ScreenCaptureKit │   X11/PipeWire     │
│  Win32/UIA      │   Cocoa/CGEvent  │     XTest/libinput │
└─────────────────────────────────────────────────────────┘
```

---

## 目录结构

```
wingman/
├── apps/
│   ├── runtime/          # 运行时 (CLI + 脚本执行)
│   │   ├── src/
│   │   │   └── main.cpp  # 入口 (支持 Lua/Python)
│   │   └── CMakeLists.txt
│   └── inspector/        # 检查工具 (Tauri 2.0 + Svelte 5)
│       ├── src/          # Svelte 5 前端
│       └── src-tauri/    # Rust 后端
│
├── lib/wingman/          # 核心库
│   ├── include/wingman/
│   │   ├── script/       # IScriptEngine, ScriptValue, ModuleDescriptor
│   │   ├── screen.hpp    # 屏幕捕获
│   │   ├── platform/iinput.hpp # 输入模拟
│   │   ├── trigger.hpp   # 触发器
│   │   ├── vision.hpp    # 视觉识别
│   │   ├── behavior_tree.hpp # 行为树
│   │   └── ocr.hpp       # OCR
│   ├── src/
│   │   └── script/modules/ # 25+ 语言无关模块实现
│   └── tests/
│
├── libs/                 # 内部辅助库
│   ├── transport/        # 网络传输 (TCP/WebSocket)
│   ├── lua/              # Lua 引擎 (sol2, 实现 IScriptEngine)
│   ├── python/           # Python 引擎 (pybind11+CPython, 实现 IScriptEngine)
│   └── proto/            # Protobuf
│
├── examples/             # 示例脚本
├── docs/                 # 文档
└── CMakeLists.txt        # 根 CMake
```

---

## 核心功能

| 功能模块 | 说明 |
|---------|------|
| **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 (OpenCV) |
| **输入模拟** | 鼠标点击/移动、按键发送、文本输入 |
| **人性化模拟** | 贝塞尔曲线鼠标移动、随机延迟、自然操作 |
| **窗口管理** | 查找窗口、激活窗口、获取位置 |
| **触发器系统** | 像素/图像/时间条件触发，自动执行动作 |
| **宏录制回放** | 录制鼠标键盘操作，保存为脚本回放 |
| **UI Automation** | Windows UIA 自动化，操作 UI 控件 |
| **OCR 识别** | Tesseract 文字识别 |
| **多语言脚本** | Lua (sol2) 和 Python (pybind11) 双引擎 |
| **EmmyLua 调试** | 支持 VS Code 断点调试 Lua 脚本 |

---

## 快速开始

### 环境要求

**Windows:**
- Windows 10/11 (x64)
- Visual Studio 2022

**macOS:**
- macOS 12+ (Monterey 或更高)
- Xcode 14+ 或 Clang

**Linux:**
- Ubuntu 22.04+ 或等效发行版
- GCC 11+ 或 Clang 14+

**通用:**
- CMake 3.20+
- vcpkg (依赖管理)

### 安装 vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### 编译

**Windows:**
```bash
cmake -B build -G "Visual Studio 17 2022" ^
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

**启用 Python 支持:**
```bash
cmake -B build -DWINGMAN_ENABLE_PYTHON=ON ...
```

**macOS:**
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

**Linux:**
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### 运行脚本

```bash
# Lua 脚本
.\build\apps\runtime\Release\wingman-runtime.exe script examples\hello.lua

# Python 脚本 (需要启用 WINGMAN_ENABLE_PYTHON)
.\build\apps\runtime\Release\wingman-runtime.exe script examples\hello.py

# 启动服务模式
.\build\apps\runtime\Release\wingman-runtime.exe start

# 启动 GUI
.\build\apps\runtime\Release\wingman-runtime.exe
```

---

## 脚本示例

### Lua

```lua
local wingman = require("wingman")

-- 截图并保存
local screenshot = wingman.screen.capture(0, 0, 1920, 1080)
screenshot:save("screenshot.png")

-- 查找颜色
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, p in ipairs(points) do
        wingman.input.click(p.x, p.y, "left")
    end
end

-- 图像匹配
local match = wingman.screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
if match then
    wingman.input.move(match.x, match.y, 500)
    wingman.input.click(match.x, match.y)
end
```

### Python

```python
from wingman import screen, input

# 截图
screen.capture()

# 查找颜色
point = screen.findColor(0xFF0000, {"x": 0, "y": 0, "w": 1920, "h": 1080}, 10)
if point:
    input.click(point["x"], point["y"])
```

---

## 开发

### VSCode 开发环境

推荐插件：

| 插件 | 用途 |
|------|------|
| [LuaLS](https://marketplace.visualstudio.com/items?itemName=sumneko.lua) | Lua 语言支持 |
| [Python](https://marketplace.visualstudio.com/items?itemName=ms-python.python) | Python 语言支持 |
| [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) | C++ 语言支持 |
| [EmmyLua](https://marketplace.visualstudio.com/items?itemName=EmmyLuaVSCode.emmylua) | Lua 调试支持 |
| [Svelte](https://marketplace.visualstudio.com/items?itemName=svelte.svelte-vscode) | Svelte 前端开发 |

配置 `.vscode/settings.json`:
```json
{
  "Lua.workspace.library": ["${workspaceFolder}/libs/lua/defs"],
  "Lua.diagnostics.globals": ["wingman"]
}
```

### 单元测试

```bash
cmake -B build -DWINGMAN_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --config Release
```

---

## 文档

- [构建指南](BUILD.md)
- [在线文档](https://cuihairu.github.io/wingman/)
  - [快速开始](https://cuihairu.github.io/wingman/guide/getting-started)
  - [API 参考](https://cuihairu.github.io/wingman/api/)
  - [示例脚本](https://cuihairu.github.io/wingman/examples/)
  - [YOLO 模型使用指南](https://cuihairu.github.io/wingman/guides/yolo-guide)
- [架构设计](docs/architecture.md)
- [平台抽象层设计](docs/platform-abstraction-design.md)
- [开发路线图](ROADMAP.md)

---

## 许可证

[Apache-2.0](LICENSE)
