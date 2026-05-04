---
layout: home

hero:
  name: "Wingman"
  text: "游戏自动化可编程控制引擎"
  tagline: "C++ + Lua 的高性能游戏自动化框架"
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

features:
  - title: 🚀 高性能
    details: C++ 核心引擎，LuaJIT 脚本执行，毫秒级响应
  - title: 🔒 安全可靠
    details: 纯用户态运行，使用合法 Windows API，不读写游戏内存
  - title: 🎮 可编程
    details: Lua 脚本控制，无限扩展可能，支持复杂业务逻辑
  - title: 🌐 远程控制
    details: 支持 TCP Server/Client 模式，可远程统一调度
  - title: 🐛 强大的调试
    details: VS Code 插件支持，断点调试、变量查看、性能分析
  - title: 🤖 人性化模拟
    details: 贝塞尔曲线鼠标移动、随机延迟、自然操作模式

---

## 简介

**Wingman** 是一个为了游戏自动化而生的可编程控制项目。

- 基于 **C++** 开发核心引擎，提供高性能的屏幕操作和输入模拟能力
- 使用 **LuaJIT** 作为脚本引擎，灵活可扩展
- 纯**用户态**运行，使用合法 Windows API，安全可靠
- 支持**远程控制**，TCP Server/Client 模式，暴露 API 供外部调用

## 核心特性

- 📷 **屏幕操作** - 截图、像素检测、颜色匹配、图像查找
- 🖱️ **输入模拟** - 鼠标点击/移动、按键发送、文本输入
- 🪟 **窗口管理** - 查找窗口、激活窗口、获取位置
- ⚙️ **进程管理** - 启动/等待/终止进程
- 🔄 **宏录制** - 录制鼠标键盘操作，自动回放
- 🎯 **触发器系统** - 像素触发、定时触发、条件组合
- 🌐 **网络层** - TCP Server/Client，支持远程控制
- 🐛 **调试器** - VS Code 插件，断点调试、变量查看
- 🤖 **人性化模拟** - 贝塞尔曲线、随机延迟、自然操作

## 快速开始

```bash
# 克隆仓库
git clone https://github.com/cuihairu/wingman.git
cd wingman

# 编译项目（需要 CMake + Visual Studio）
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release

# 运行示例
wingman.exe scripts/examples/hello.lua
```

## 许可证

[Apache-2.0 License](https://github.com/cuihairu/wingman/blob/main/LICENSE)
