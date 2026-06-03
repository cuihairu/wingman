---
title: 快速开始
---

# 快速开始

## 环境要求

### 开发环境

- **Windows 10/11** - 主要支持平台
- **Visual Studio 2022** - C++17 支持
- **CMake 3.20+** - 构建系统
- **Git** - 版本控制
- **vcpkg** - C++ 包管理器

### Python 环境（可选）

如需使用 Python 脚本：
- **Python 3.8+**
- **pybind11**

### vcpkg 安装

```bash
# 1. 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 2. 运行引导脚本
.\bootstrap-vcpkg.bat

# 3. 集成到系统（自动关联到 Visual Studio）
.\vcpkg integrate install

# 4. 设置环境变量（可选，用于自动 triplet 检测）
set VCPKG_ROOT=C:\path\to\vcpkg
```

## 项目依赖

Wingman 使用以下 vcpkg 包：

| 包名 | 用途 |
|-----|------|
| `lua` | Lua 脚本引擎 |
| `opencv4` | 图像处理（可选） |
| `spdlog` | 日志库 |
| `nlohmann-json` | JSON 配置解析 |
| `asio` | 网络库 |
| `sol2` | C++/Lua 绑定（header-only） |

## 编译项目

### 1. 克隆仓库

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman
```

### 2. 安装依赖

```bash
# 安装所有依赖
vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio

# 或使用 vcpkg.json 自动安装
vcpkg install
```

### 3. 使用 CMake 生成项目

```bash
build-scripts\configure-msvc-ninja.bat
```

### 4. 编译

```bash
build-scripts\build-runtime-msvc-ninja.bat
```

### 5. 运行

```bash
# 运行 Lua 示例脚本
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe script scripts\examples\hello.lua

# 运行 Python 示例脚本
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe script scripts\examples\hello.py
```

## vcpkg.json

项目根目录包含 `vcpkg.json`，声明依赖：

```json
{
  "dependencies": [
    {
      "name": "lua",
      "features": ["tool"]
    },
    "opencv4",
    {
      "name": "spdlog",
      "features": ["wchar"]
    },
    "nlohmann-json",
    "asio"
  ]
}
```

## 命令行参数

```bash
# 执行脚本
wingman-runtime.exe script script.lua
wingman-runtime.exe script script.py

# 启动 Agent（读取 agent.toml）
wingman-runtime.exe start

# 打包单文件脚本运行时
wingman-runtime.exe build --script script.lua --output script-runtime.exe

# 帮助信息
wingman-runtime.exe --help
```

## 第一个脚本

### Python 版本

创建文件 `hello.py`：

```python
from wingman import screen, util

# 打印问候
print("Hello from Wingman!")

# 获取屏幕尺寸
width, height = screen.get_size()
print(f"Screen size: {width}x{height}")

# 延迟 1 秒
util.sleep(1000)

print("Script completed!")
```

### Lua 版本

创建文件 `hello.lua`：

```lua
local screen = require("wingman.screen")
local util = require("wingman.util")

-- 打印问候
print("Hello from Wingman!")

-- 获取屏幕尺寸
local width, height = screen.getSize()
print(string.format("Screen size: %dx%d", width, height))

-- 延迟 1 秒
util.sleep(1000)

print("Script completed!")
```

运行：

```bash
# Python
wingman-runtime.exe script hello.py

# Lua
wingman-runtime.exe script hello.lua
```

## 选择脚本语言

Wingman 同时支持 Python 和 Lua，如何选择？

### 使用 Python 如果你：
- 熟悉 Python 语法
- 需要使用丰富的 Python 库
- 需要更好的 IDE 支持（类型提示、自动补全）
- 开发复杂的项目

### 使用 Lua 如果你：
- 需要最快的启动速度
- 内存受限的环境
- 偏好简洁的语法

## 开发容器（可选）

使用 Dev Container 在 VS Code 中开发：

```bash
# 在容器中打开
code .
```

## 下一步

- [API 参考](/api/) - 查看完整 API 文档
- [示例脚本](/examples/) - 学习更多用法
- [调试指南](/guide/debugging) - 如何调试脚本
- [远程控制](/guide/remote) - 使用 TCP 远程控制
- [YOLO 模型使用](/guides/yolo-guide) - 使用 YOLO 进行目标检测
