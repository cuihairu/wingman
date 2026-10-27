> [中文](../zh/guide/introduction.md) | **English**

# Project Introduction

## What is Wingman?

**Wingman** is a programmable game automation control engine built with C++, supporting both Lua and Python as scripting languages.

## Design Goals

### Core Goals

1. **High performance** - C++ core engine with Lua/Python script execution and millisecond-level response
2. **Safe and reliable** - Runs purely in user mode using legitimate Windows APIs
3. **Programmable** - Dual-language support (Lua and Python) with unlimited extension possibilities
4. **Easy to debug** - VS Code extension support with breakpoint debugging, variable inspection, and profiling

### What We Don't Do

Wingman follows the single-responsibility principle; the following features are **out of scope**:

| Feature | Notes |
|-----|------|
| ❌ Kernel drivers | No driver development; user mode only |
| ❌ Memory read/write | Does not read or modify game memory |
| ❌ DLL injection | Does not inject code into game processes |
| ❌ Game cracking | Does not modify game files or protocols |
| ❌ Distributed networking | No distributed networking |
| ❌ Account management | Does not manage game accounts |
| ❌ Runtime WebSocket/HTTP control plane | The local UI uses IPC; remote orchestration uses the Go server |

## Project Boundaries

```
┌─────────────────────────────────────────────────────────────────┐
│                      ✅ Wingman 边界内                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   屏幕操作           截图、像素检测、颜色匹配、图像查找            │
│   输入模拟           鼠标点击/移动、按键发送、文本输入              │
│   窗口管理           查找窗口、激活窗口、获取位置                   │
│   进程管理           启动/等待/终止进程                            │
│   UI Automation     直接操作 Windows 控件                         │
│   脚本引擎           Lua、Python 双语言支持                       │
│   机器学习           ONNX 模型推理（YOLO 检测等）                   │
│   事件系统           跨模块事件订阅与发布                           │
│   工具函数           延迟、日志、配置                               │
│   人性化模拟         随机延迟、自然鼠标轨迹、操作节奏                │
│   远程编排           Runtime 主动连接 Go server，由 Go server 中控    │
│   调试器             VS Code 插件，断点、变量查看、性能分析          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                      ❌ Wingman 边界外                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   内核驱动            不开发驱动，纯用户态                         │
│   内存读写            不读取/修改游戏内存                           │
│   DLL 注入           不注入任何代码到游戏进程                       │
│   游戏破解            不修改游戏文件/协议                          │
│   分布式系统          不做分布式网络                              │
│   账号管理            不管理账号                                  │
│   Runtime 网络控制面  不暴露 WebSocket/HTTP server 供 UI 控制        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Core Design Principles

### 1. Safety First

- Runs purely in user mode; never enters the kernel
- Uses legitimate Windows APIs (SendInput, etc.)
- Does not read or write game memory
- Does not inject any code into game processes
- Simulates real human operation patterns

### 2. Performance First

- Performance-sensitive operations implemented in C++
- Efficient script execution in Lua and Python
- Zero-copy data passing
- Batched operations to reduce cross-language calls

### 3. Multi-Language Support

- **Lua** - Lightweight, high performance, well suited to game scripting
- **Python** - Rich third-party ecosystem, well suited to complex logic and AI integration
- Unified API design; both languages are called the same way

### 4. Programmability

- Script-driven control, extensible by users
- No restrictions on use cases
- Rich API surface exposed
- Supports complex business logic

### 5. Humanization

- Simulates real human operations
- Reduces detection risk
- Configurable "humanization" parameters

## Target Users

| User Type | How They Use It |
|---------|---------|
| **Game studios** | Write Lua/Python scripts for automation |
| **Advanced players** | Write personal assist scripts |
| **AI developers** | Integrate models such as YOLO for intelligent recognition |

## Tech Stack

```
核心引擎
├── C++17
├── Windows API
├── Lua (Lua 5.5)
├── Python (CPython)
├── OpenCV (可选，用于图像处理)
├── ONNX Runtime (可选，用于 ML 推理)
├── pybind11 (C++/Python 绑定)
└── sol2 (C++/Lua 绑定)

构建系统
├── CMake
└── Visual Studio 2022

包管理
└── vcpkg

调试器
├── Debug Adapter Protocol
└── VS Code 插件

文档
└── VitePress
```

## Why Two Scripting Languages?

### Lua's Advantages

- **Lightweight** - Minimal memory footprint
- **High performance** - Millisecond-level startup time
- **Simple** - Concise syntax, easy to learn
- **Embeddable** - Designed for embedding from day one

### Python's Advantages

- **Third-party libraries** - Broad coverage, installable directly via pip
- **AI libraries** - Can hook up PyTorch and TensorFlow models
- **Development efficiency** - Mature IDE support, breakpoint debugging in VS Code
- **Widely used** - Familiar to a large developer base

### Usage Recommendations

- **Game scripts** - Lua recommended, performance first
- **AI integration** - Python recommended, rich third-party ecosystem
- **Rapid prototyping** - Python recommended, higher development efficiency
- **Production** - Choose based on your team's tech stack

## License

Apache License 2.0 - see [LICENSE](https://github.com/cuihairu/wingman/blob/main/LICENSE) for details
