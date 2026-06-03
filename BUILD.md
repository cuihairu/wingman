# Wingman 构建指南

## 系统要求

### Windows
- Windows 10/11 (x64)
- Visual Studio 2022
- CMake 3.20+
- vcpkg 包管理器
- Git

### Clasp (本地开发依赖)

Wingman 使用 [Clasp](https://github.com/cuihairu/clasp) 作为命令行库。通过 **overlay ports** 本地管理：

#### 方式1: Clasp 在本地开发目录

```bash
# clasp 位于 sibling 目录 (如: C:\Users\你的用户名\Workspaces\clasp)
# 或设置环境变量
set CLASP_SOURCE_DIR=C:\path\to\clasp
```

#### 方式2: 添加为 Git Submodule

```bash
cd wingman
git submodule add https://github.com/cuihairu/clasp.git libs/clasp
git submodule update --init --recursive
```

## 快速开始

### 1. 安装 vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

### 2. 配置项目

```bash
# 克隆项目
git clone https://github.com/cuihairu/wingman.git
cd wingman

# 配置 MSVC + Ninja + vcpkg
build-scripts\configure-msvc-ninja.bat

# 如果 clasp 不在默认位置，设置环境变量
# $env:CLASP_SOURCE_DIR = "C:\path\to\clasp"
```

### 3. 编译

```bash
build-scripts\build-runtime-msvc-ninja.bat
```

### 4. 运行

```bash
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe
```

## 测试

### 启用测试

```bash
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake -S . -B build-msvc-ninja-vcpkg -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:\Users\admin\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DWINGMAN_BUILD_TESTS=ON -DBUILD_CORE_TESTS=ON -DBUILD_TRANSPORT_TESTS=ON"
```

### 运行测试

```bash
# 核心库测试 (25 个测试)
.\build-msvc-ninja-vcpkg\lib\wingman\tests\core_tests.exe

# 传输库测试 (7 个测试)
.\build-msvc-ninja-vcpkg\libs\transport\tests\transport_tests.exe

# 协议库测试 (7 个测试)
.\build-msvc-ninja-vcpkg\libs\proto\tests\proto_tests.exe

# 调试库测试 (4 个测试)
.\build-msvc-ninja-vcpkg\libs\debug\tests\debug_tests.exe
```

## 性能基准测试

### 编译基准测试

```bash
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake -S . -B build-msvc-ninja-vcpkg -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:\Users\admin\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DWINGMAN_BUILD_BENCHMARKS=ON"
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake --build build-msvc-ninja-vcpkg --target kvstore_bench --config Debug"
```

### 运行基准测试

```bash
.\build-msvc-ninja-vcpkg\lib\wingman\tests\benchmarks\kvstore_bench.exe
```

### 性能指标

| 操作 | 性能 |
|------|------|
| SET | 0.04 μs/op |
| GET | 0.03 μs/op |
| DELETE | 0.21 μs/op |
| INCR | 0.07 μs/op |
| HSET | 0.04 μs/op |
| HGET | 0.04 μs/op |
| LPUSH | 0.12 μs/op |
| LPOP | 0.20 μs/op |
| WRITE 吞吐量 | 10M ops/sec |

## 构建选项

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `WINGMAN_ENABLE_OCR` | OFF | 启用 OCR 支持 (需要 Tesseract) |
| `WINGMAN_ENABLE_ML` | OFF | 启用 ML/AI 支持 (需要 ONNX Runtime) |
| `WINGMAN_BUILD_TESTS` | OFF | 构建测试 |
| `WINGMAN_BUILD_BENCHMARKS` | OFF | 构建基准测试 |
| `BUILD_CORE_TESTS` | OFF | 构建核心库测试 |
| `BUILD_TRANSPORT_TESTS` | OFF | 构建传输库测试 |
| `BUILD_PROTO_TESTS` | OFF | 构建协议库测试 |
| `BUILD_DEBUG_TESTS` | OFF | 构建调试库测试 |

### Vcpkg Features

```bash
# 启用 tests feature (安装 GTest)
-DVCPKG_MANIFEST_FEATURES=tests

# 启用多个 features
-DVCPKG_MANIFEST_FEATURES=tests;ocr
```

## 依赖项

### 核心依赖 (自动安装)
- clasp (命令行库，通过 overlay ports)
- lua 5.4
- spdlog
- nlohmann-json
- asio
- curl
- sqlite3
- protobuf

### 可选依赖
- tesseract (OCR)
- onnxruntime (ML/AI)
- gtest (测试)

## 故障排除

### vcpkg 安装缓慢

使用二进制缓存：

```bash
$env:VCPKG_DEFAULT_BINARY_CACHE = "C:\vcpkg\archives"
```

### CMake 找不到包

确保使用正确的 toolchain 文件和 triplet：

```bash
-DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
-DVCPKG_TARGET_TRIPLET=x64-windows-static
```

### 链接错误

确保使用静态运行时库，项目默认配置已设置。

## CI/CD

项目使用 GitHub Actions 进行持续集成：

```yaml
# .github/workflows/ci.yml
- Windows 构建
- C++ 测试
- Lua 测试
- 代码覆盖率
```

## CI Status

- Windows: build, run C++ tests, generate coverage, upload to Codecov
- Linux/macOS: compile-only validation
- Go server: run `go test ./...` on Windows
