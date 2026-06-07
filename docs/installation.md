# 安装指南

本指南提供 Wingman 在不同平台上的详细安装说明。

## 📋 目录

- [系统要求](#系统要求)
- [安装 vcpkg](#安装-vcpkg)
- [Windows 安装](#windows-安装)
- [macOS 安装](#macos-安装)
- [Linux 安装](#linux-安装)
- [启用额外功能](#启用额外功能)
- [故障排除](#故障排除)

## 系统要求

### 最低要求

| 平台 | 操作系统 | 编译器 | 其他 |
|------|----------|--------|------|
| Windows | Windows 10/11 (x64) | Visual Studio 2022 | CMake 3.20+ |
| macOS | macOS 12+ | Xcode 14+ 或 Clang | CMake 3.20+ |
| Linux | Ubuntu 22.04+ | GCC 11+ 或 Clang 14+ | CMake 3.20+ |

### 通用依赖

- **CMake**: 3.20 或更高版本
- **vcpkg**: C++ 包管理器
- **Git**: 用于克隆源代码

## 安装 vcpkg

vcpkg 是 Wingman 使用的 C++ 包管理器。

### Windows

```powershell
# 克隆 vcpkg 仓库
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg

# 运行引导脚本
C:\vcpkg\bootstrap-vcpkg.bat

# 集成到系统
C:\vcpkg\vcpkg integrate install
```

### macOS/Linux

```bash
# 克隆 vcpkg 仓库
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg

# 运行引导脚本
~/vcpkg/bootstrap-vcpkg.sh

# 集成到系统
~/vcpkg/vcpkg integrate install
```

### 验证安装

```bash
vcpkg version
```

应该显示 vcpkg 版本信息。

## Windows 安装

### 方法一：使用构建脚本（推荐）

```powershell
build-scripts\build-runtime-msvc-ninja.bat
```

此脚本会自动：
1. 配置 CMake
2. 安装依赖
3. 编译项目
4. 生成可执行文件

### 方法二：手动编译

#### 1. 安装 Visual Studio 2022

确保安装了以下组件：
- MSVC v143 - VS 2022 C++ x64/x86 编译器
- Windows 10/11 SDK
- C++ CMake tools for Visual Studio

#### 2. 克隆源代码

```powershell
git clone https://github.com/cuihairu/wingman.git
cd wingman
```

#### 3. 配置和编译

**使用 Ninja（推荐，更快）:**
```powershell
cmake -B build -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

**使用 Visual Studio 生成器:**
```powershell
cmake -B build -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

#### 4. 验证安装

```powershell
.\build\apps\runtime\wingman-runtime.exe --version
```

## macOS 安装

### 1. 安装 Xcode 命令行工具

```bash
xcode-select --install
```

### 2. 克隆源代码

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman
```

### 3. 配置和编译

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 4. 验证安装

```bash
./build/apps/runtime/wingman-runtime --version
```

## Linux 安装

### Ubuntu/Debian

#### 1. 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build
```

#### 2. 克隆源代码

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman
```

#### 3. 配置和编译

```bash
cmake -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build --config Release
```

#### 4. 验证安装

```bash
./build/apps/runtime/wingman-runtime --version
```

## 启用额外功能

### Python 支持

启用 Python 脚本引擎：

```bash
cmake -B build -DWINGMAN_ENABLE_PYTHON=ON ...
```

**要求**:
- Python 3.11+
- pybind11（通过 vcpkg 安装）

### OCR 支持

启用 Tesseract OCR：

```bash
cmake -B build -DWINGMAN_ENABLE_OCR=ON ...
```

### ML/AI 支持

启用 ONNX Runtime：

```bash
cmake -B build -DWINGMAN_ENABLE_ML=ON ...
```

### 构建测试

启用单元测试：

```bash
cmake -B build -DWINGMAN_BUILD_TESTS=ON \
    -DBUILD_CORE_TESTS=ON ...
```

## 故障排除

### Windows

#### CMake 找不到 vcpkg

**问题**: `CMake Error: Could not find CMAKE_TOOLCHAIN_FILE`

**解决方案**:
1. 检查 vcpkg 路径是否正确
2. 确保路径使用绝对路径
3. 验证 vcpkg 已成功安装

#### 编译器错误

**问题**: MSVC 编译器错误

**解决方案**:
1. 确保 Visual Studio 2022 已正确安装
2. 安装最新的 Windows SDK
3. 使用 Developer PowerShell 提示符

### macOS

#### Xcode 未找到

**问题**: `xcode-select: error: tool 'xcodebuild' requires Xcode`

**解决方案**:
```bash
xcode-select --install
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

#### 权限错误

**问题**: 编译时权限被拒绝

**解决方案**:
```bash
sudo chown -R $(whoami) ~/vcpkg
```

### Linux

#### 缺少系统依赖

**问题**: 缺少头文件或库

**解决方案**:
```bash
sudo apt install -y build-essential cmake git ninja-build \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxi-dev libgl1-mesa-dev libglu1-mesa-dev
```

#### vcpkg 依赖失败

**问题**: vcpkg 无法安装某些包

**解决方案**:
```bash
sudo apt install -y g++ gcc rsync
```

## 🎯 下一步

安装完成后：

1. **运行第一个脚本**: 查看 [快速开始](getting-started.md)
2. **学习 API**: 浏览 [API 参考](api/overview.md)
3. **查看示例**: 探索 `examples/` 目录

## 📞 获取帮助

如果遇到问题：

- 查看 [常见问题](../FAQ.md)
- 提交 [GitHub Issue](https://github.com/cuihairu/wingman/issues)
- 加入 [GitHub Discussions](https://github.com/cuihairu/wingman/discussions)

---

**相关文档**: [快速开始](getting-started.md) | [开发指南](development.md) | [构建系统](build-system.md)
