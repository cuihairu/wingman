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

[文档](docs/README.md) · [快速开始](docs/getting-started.md) · [API 参考](docs/api/overview.md) · [示例](docs/examples/)

</div>

> ⚠️ **免责声明**
>
> 本工具仅供合法场景使用，包括但不限于：自动化测试、可单机游戏辅助、无障碍辅助等。
> 使用本工具违反任何游戏或软件的用户协议所导致的后果，由使用者自行承担。
> 作者不对因使用本工具而产生的任何法律责任负责。

---

## ✨ 核心特性

- **🚀 高性能** - C++ 核心引擎，Lua/Python 脚本执行，毫秒级响应
- **🐍 多语言** - 同时支持 Lua 和 Python，统一 API 接口
- **🔒 安全可靠** - 纯用户态运行，使用合法平台 API，不读写游戏内存
- **🎮 可编程** - 脚本控制，灵活扩展，支持复杂业务逻辑
- **🌐 跨平台** - 支持 Windows、macOS、Linux，统一接口抽象

### 功能模块

| 模块 | 功能 |
|------|------|
| 🖥️ **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 |
| 🖱️ **输入模拟** | 鼠标点击/移动、按键发送、文本输入 |
| 🪟 **窗口管理** | 查找窗口、激活窗口、获取位置 |
| ⚡ **触发器系统** | 像素/图像/时间条件触发，自动执行动作 |
| 📼 **宏录制** | 录制鼠标键盘操作，保存为脚本回放 |
| 🤖 **UI Automation** | Windows UIA 自动化，操作 UI 控件 |
| 📖 **OCR 识别** | Tesseract 文字识别（可选依赖） |
| 💾 **数据持久化** | kv 键值存储、SQLite 数据库 |
| 📄 **序列化格式** | JSON、INI 配置文件解析 |
| 🐛 **调试支持** | VS Code 断点调试 Lua 脚本（需启用调试组件） |

> 部分高级模块（OCR、ML/YOLO、远程编排、脚本调试器）依赖可选组件或仍处于持续建设中。默认可用能力以当前构建配置、运行时参数和对应 API 文档为准。

---

## 🚀 快速开始

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
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe script examples\hello.lua
```

**详细安装指南请查看 [安装文档](docs/installation.md)**

---

## 💻 代码示例

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

## 📚 文档

- [快速开始](docs/getting-started.md) - 5 分钟上手指南
- [安装指南](docs/installation.md) - 详细的安装和配置说明
- [API 参考](docs/api/overview.md) - 完整的 API 文档
- [架构设计](docs/architecture.md) - 系统架构和设计决策
- [开发指南](docs/DEVELOPMENT.md) - 贡献和开发指南

---

## 🏗️ 架构概览

Wingman 采用 **C++ 核心引擎 + 多语言脚本** 的架构设计：

- **控制面**: Go server 作为远程中控，runtime 作为 agent 主动连接
- **本地控制**: Tauri GUI 通过本地 IPC 控制 runtime
- **脚本引擎**: Lua (sol2) 和 Python (pybind11) 统一接口
- **模块化**: 25+ 语言无关模块，易于扩展

详细架构说明请查看 [架构文档](docs/architecture.md)

---

## 🤝 贡献

欢迎贡献！请查看 [开发指南](docs/DEVELOPMENT.md) 了解详情。

---

## 📄 许可证

[Apache-2.0](LICENSE)

---

<div align="center">

**[⬆ 返回顶部](#wingman)**

</div>
