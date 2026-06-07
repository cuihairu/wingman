# 快速开始

欢迎来到 Wingman！本指南将帮助你在 5 分钟内完成安装并运行第一个脚本。

## 📋 前置要求

在开始之前，请确保你的系统满足以下要求：

### Windows
- Windows 10/11 (x64)
- Visual Studio 2022
- CMake 3.20+

### macOS
- macOS 12+ (Monterey 或更高)
- Xcode 14+ 或 Clang
- CMake 3.20+

### Linux
- Ubuntu 22.04+ 或等效发行版
- GCC 11+ 或 Clang 14+
- CMake 3.20+

### 通用工具
- **vcpkg**: C++ 包管理器

## 🔧 安装 vcpkg

### Windows

```powershell
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### macOS/Linux

```bash
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
~/vcpkg/vcpkg integrate install
```

## 🚀 快速安装

### Windows（推荐）

使用提供的构建脚本：

```powershell
build-scripts\build-runtime-msvc-ninja.bat
```

### 手动编译

**Windows:**
```powershell
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build
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

详细安装说明请查看 [安装指南](installation.md)。

## 🎯 运行第一个脚本

### 创建你的第一个 Lua 脚本

创建文件 `hello.lua`：

```lua
local wingman = require("wingman")

print("Hello, Wingman!")

-- 截图
local screenshot = wingman.screen.capture(0, 0, 1920, 1080)
print("截图成功！尺寸: " .. screenshot:width() .. "x" .. screenshot:height())

-- 保存截图
screenshot:save("hello.png")
print("截图已保存到 hello.png")
```

### 运行脚本

**Windows:**
```powershell
.\build\apps\runtime\wingman-runtime.exe script hello.lua
```

**macOS/Linux:**
```bash
./build/apps/runtime/wingman-runtime script hello.lua
```

### 创建你的第一个 Python 脚本

**注意**: Python 支持需要在编译时启用 `WINGMAN_ENABLE_PYTHON=ON`

创建文件 `hello.py`：

```python
from wingman import screen

print("Hello, Wingman!")

# 截图
screenshot = screen.capture(0, 0, 1920, 1080)
print(f"截图成功！尺寸: {screenshot.width()}x{screenshot.height()}")

# 保存截图
screenshot.save("hello.png")
print("截图已保存到 hello.png")
```

运行：
```bash
wingman-runtime script hello.py
```

## 📚 下一步

现在你已经成功运行了第一个脚本！接下来可以：

- **📖 学习更多**：查看 [Lua 脚本指南](guides/lua-scripting.md) 或 [Python 脚本指南](guides/python-scripting.md)
- **🔍 探索 API**：浏览 [API 参考](api/overview.md) 了解所有可用功能
- **💾 数据存储**：学习如何使用 [数据库](guides/database.md) 和 [配置文件](guides/configuration.md)
- **🎯 高级功能**：查看 [触发器系统](guides/triggers.md) 和 [图像识别](guides/image-recognition.md)

## 🎨 更多示例

查看 `examples/` 目录获取更多示例脚本：

- **基础示例**: 屏幕截图、鼠标点击、键盘输入
- **图像识别**: 颜色查找、图像匹配
- **数据存储**: SQLite 数据库、键值存储
- **触发器**: 自动化触发和执行
- **高级功能**: OCR、UI Automation

## ❓ 常见问题

### 编译错误

如果遇到编译错误，请检查：
1. Visual Studio/Xcode 是否正确安装
2. vcpkg 是否正确安装和集成
3. CMake 版本是否 >= 3.20

详细排查请查看 [安装指南](installation.md) 的故障排除部分。

### 脚本运行错误

如果脚本运行出错：
1. 检查脚本语法是否正确
2. 确认模块名称拼写正确
3. 查看 [API 文档](api/overview.md) 确认函数用法

### 需要帮助？

- 提交 [GitHub Issue](https://github.com/cuihairu/wingman/issues)
- 查看 [示例代码](../examples/)
- 阅读详细文档

---

**继续探索**: [API 参考](api/overview.md) | [使用指南](guides/lua-scripting.md) | [示例](../examples/)
