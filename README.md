# Wingman

<div align="center">

**游戏自动化可编程控制引擎**

C++ + Lua 的高性能游戏自动化框架

[![Build](https://github.com/cuihairu/wingman/workflows/Build/badge.svg)](https://github.com/cuihairu/wingman/actions/workflows/build.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[文档](https://cuihairu.github.io/wingman/) | [快速开始](#快速开始) | [API](https://cuihairu.github.io/wingman/api/) | [示例](https://cuihairu.github.io/wingman/examples/)

</div>

## 简介

**Wingman** 是一个游戏自动化可编程控制引擎。

- 基于 **C++** 开发核心引擎，提供高性能的屏幕操作和输入模拟能力
- 使用 **LuaJIT** 作为脚本引擎，灵活可扩展
- 纯**用户态**运行，使用合法 Windows API，安全可靠
- 支持**远程控制**，TCP Server/Client 模式，暴露 API 供外部调用

## 特性

- 🚀 **高性能** - C++ 核心引擎，LuaJIT 脚本执行，毫秒级响应
- 🔒 **安全可靠** - 纯用户态运行，使用合法 Windows API，不读写游戏内存
- 🎮 **可编程** - Lua 脚本控制，无限扩展可能，支持复杂业务逻辑
- 🌐 **远程控制** - 支持 TCP Server/Client 模式，可远程统一调度
- 🐛 **强大的调试** - VS Code 插件支持，断点调试、变量查看、性能分析
- 🤖 **人性化模拟** - 贝塞尔曲线鼠标移动、随机延迟、自然操作模式

## 核心功能

| 模块 | 功能 |
|-----|------|
| **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 |
| **输入模拟** | 鼠标点击/移动、按键发送、文本输入 |
| **窗口管理** | 查找窗口、激活窗口、获取位置 |
| **进程管理** | 启动/等待/终止进程 |
| **宏录制** | 录制鼠标键盘操作，自动回放 |
| **触发器系统** | 像素触发、定时触发、条件组合 |
| **网络层** | TCP Server/Client，支持远程控制 |
| **调试器** | VS Code 插件，断点调试、变量查看 |
| **人性化模拟** | 贝塞尔曲线、随机延迟、自然操作 |

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
vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio

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

## 文档

详细文档请访问 [https://cuihairu.github.io/wingman/](https://cuihairu.github.io/wingman/)

- [指南](https://cuihairu.github.io/wingman/guide/)
- [API 参考](https://cuihairu.github.io/wingman/api/)
- [示例](https://cuihairu.github.io/wingman/examples/)

## 许可证

[Apache-2.0](LICENSE)
