# Wingman

<div align="center">

<img src="docs/public/logo.svg" alt="Wingman" width="100" />

**游戏自动化可编程控制引擎**

C++ + Lua 的高性能游戏自动化框架

[![Windows](https://img.shields.io/badge/OS-Windows-blue.svg)](https://github.com/cuihairu/wingman)
[![CI](https://github.com/cuihairu/wingman/workflows/CI/badge.svg)](https://github.com/cuihairu/wingman/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cuihairu/wingman/branch/main/graph/badge.svg)](https://codecov.io/gh/cuihairu/wingman)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Lua](https://img.shields.io/badge/Lua-5.5-blue.svg)](https://www.lua.org/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[文档](https://cuihairu.github.io/wingman/) | [快速开始](#快速开始) | [API](https://cuihairu.github.io/wingman/api/) | [示例](https://cuihairu.github.io/wingman/examples/)

</div>

> ⚠️ **免责声明**
>
> 本工具仅供合法场景使用，包括但不限于：自动化测试、可单机游戏辅助、无障碍辅助等。
> 使用本工具违反任何游戏或软件的用户协议所导致的后果，由使用者自行承担。
> 作者不对因使用本工具而产生的任何法律责任负责。
>

## 简介

**Wingman** 是一个游戏自动化可编程控制引擎。

- 基于 **C++** 开发核心引擎，提供高性能的屏幕操作和输入模拟能力
- 使用 **Lua** 作为脚本引擎，灵活可扩展
- 纯**用户态**运行，使用合法 Windows API，安全可靠
- 支持**远程控制**，TCP Server/Client 模式，暴露 API 供外部调用

## 特性

- 🚀 **高性能** - C++ 核心引擎，Lua 脚本执行，毫秒级响应
- 🔒 **安全可靠** - 纯用户态运行，使用合法 Windows API，不读写游戏内存
- 🎮 **可编程** - Lua 脚本控制，无限扩展可能，支持复杂业务逻辑
- 🌐 **远程控制** - 支持 TCP Server/Client 模式，可远程统一调度
- 🐛 **强大的调试** - VS Code 插件支持，断点调试、变量查看、性能分析
- 🤖 **人性化模拟** - 贝塞尔曲线鼠标移动、随机延迟、自然操作模式

## 核心功能

| 模块 | 功能 |
|-----|------|
| **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 (OpenCV) |
| **ML/AI 推理** | ONNX Runtime 模型推理，支持图像分类、目标检测、分割 |
| **UI Automation** | Windows UIA 自动化，直接操作 UI 控件（按钮/编辑框/列表等） |
| **Vision 视觉** | 颜色检测、图像匹配、边缘检测、轮廓检测、形状识别 |
| **OCR 识别** | Tesseract 文字识别，支持多语言 |
| **智能触发器** | SmartTrigger 系统，支持颜色/图像/文字/OCR 条件触发 |
| **行为树引擎** | BehaviorTree，支持 Sequence/Selector/Parallel/Wait/Retry 节点 |
| **输入模拟** | 鼠标点击/移动、按键发送、文本输入 |
| **人性化模拟** | 贝塞尔曲线鼠标移动、随机延迟、自然操作 |
| **窗口管理** | 查找窗口、激活窗口、获取位置 |
| **进程管理** | 启动/等待/终止进程 |
| **HTTP 客户端** | GET/POST/PUT/DELETE 请求，表单提交 (libcurl) |
| **JSON 封装** | JSON 解析和序列化 (nlohmann/json) |
| **存储系统** | 四层存储架构：SessionStorage/LocalStorage/TeamStorage/ServerStorage |
| **组队编排** | 多 Client 协同组队、投票协调 |
| **宏录制** | 录制鼠标键盘操作，保存为 Lua/JSON 回放 |
| **触发器系统** | 颜色/图像/窗口/进程触发，自动执行动作 |
| **Web Dashboard** | React + UmiJS + Ant Design 管理界面 |

## 快速开始

### 环境要求

- Windows 10/11
- Visual Studio 2022
- CMake 3.20+
- vcpkg

### 安装 vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### 编译

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman

# 安装依赖
vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio curl

# 配置项目
cmake -B build -S . -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build build --config Release
```

### 运行

```bash
.\build\Release\wingman.exe scripts\examples\hello.lua
```

## 开发

### C++ 单元测试

```bash
# 构建测试
cmake -B build -DWINGMAN_BUILD_TESTS=ON
cmake --build build --config Debug

# 运行测试
.\build\tests\cpp\Debug\wingman_tests.exe
```

### Lua 开发工具 (可选)

Wingman 提供可选的 Lua 开发工具：

```bash
# 安装 LuaRocks 包管理器
scripts\install-luarocks.cmd

# 安装 Busted 测试框架
scripts\install-busted.cmd

# 运行 Lua 测试
scripts\run-lua-tests.cmd
```

详细开发指南请参考 [DEVELOPMENT.md](docs/DEVELOPMENT.md)。

## 文档

详细文档请访问 [https://cuihairu.github.io/wingman/](https://cuihairu.github.io/wingman/)

- [指南](https://cuihairu.github.io/wingman/guide/)
- [API 参考](https://cuihairu.github.io/wingman/api/)
- [示例](https://cuihairu.github.io/wingman/examples/)

API 示例请参考[文档](https://cuihairu.github.io/wingman/api/)。

### Web Dashboard

```bash
cd dashboard
npm install
npm run dev
```

- 脚本管理
- 窗口监控
- 系统状态
- 远程控制

## 许可证

[Apache-2.0](LICENSE)
