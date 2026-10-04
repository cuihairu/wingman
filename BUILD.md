# Wingman 构建指南

## 系统要求

### Windows
- Windows 10/11 (x64)
- Visual Studio 2022
- CMake 3.20+
- vcpkg 包管理器
- Git

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
cmake -S . -B build-tests `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DVCPKG_MANIFEST_FEATURES=tests `
  -DWINGMAN_BUILD_TESTS=ON
```

### 运行测试

```bash
cmake --build build-tests --config Debug --target core_tests runtime_tests transport_tests
ctest --test-dir build-tests -C Debug --output-on-failure
```

`WINGMAN_BUILD_TESTS=ON` 会自动启用标准 C++ 测试目标（core/agentcore/runtime/transport；启用 Python 引擎时含 python 绑定测试）。
如果还需要单独验证 Lua 绑定层，再额外加 `-DBUILD_LUA_TESTS=ON` 并构建 `lua_tests`。
建议测试使用单独的 `build-tests/` 目录，避免和现有 `build/` 的生成器或配置冲突。

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
| `WINGMAN_ENABLE_PYTHON` | OFF | 启用 Python 脚本引擎 (需要 CPython + pybind11) |
| `WINGMAN_COMPAT_BUILD` | OFF | 只构建可移植兼容目标 |
| `WINGMAN_BUILD_AGENT` | ON | 构建 Agent 多模块（同时决定 TRANSPORT/CORE/LUA/RUNTIME 默认值） |
| `WINGMAN_BUILD_TRANSPORT` / `CORE` / `LUA` / `RUNTIME` | 跟随 AGENT | 各模块单独开关 |
| `WINGMAN_BUILD_TESTS` | OFF | 构建测试 |
| `WINGMAN_BUILD_BENCHMARKS` | OFF | 构建基准测试 |
| `BUILD_CLIENT_TESTS` | OFF | 构建 runtime 应用测试 |
| `BUILD_CORE_TESTS` | OFF | 构建核心库测试 |
| `BUILD_TRANSPORT_TESTS` | OFF | 构建传输库测试 |
| `BUILD_AGENTCORE_TESTS` | OFF | 构建 agentcore 库测试 |
| `BUILD_LUA_TESTS` | OFF | 构建 Lua 绑定层测试（默认手动开启） |
| `BUILD_PYTHON_TESTS` | OFF | 构建 Python 绑定层测试 |

### Vcpkg Features

```bash
# 启用 tests feature (安装 GTest)
-DVCPKG_MANIFEST_FEATURES=tests

# 启用多个 features
-DVCPKG_MANIFEST_FEATURES=tests;ocr;ml;vision;python
```

features 映射：`tests`→gtest、`ocr`→tesseract、`ml`→onnxruntime、`vision`→opencv4、`python`→python3+pybind11。

## 依赖项

全部由 `vcpkg.json` manifest 声明，CMake 经 toolchain 自动安装，禁止系统库回退（见 CLAUDE.md）。

### 核心依赖（始终安装）
- asio、curl、lua、nlohmann-json、opencv4、openssl、sol2、spdlog、sqlite3

### 可选依赖（按 feature / 选项）
- tesseract（OCR）、onnxruntime（ML/AI）、gtest（tests feature）、python3 + pybind11（Python 引擎）

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

### macOS：Xcode 未找到

**问题**: `xcode-select: error: tool 'xcodebuild' requires Xcode`

```bash
xcode-select --install
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

### macOS：编译时权限被拒绝

```bash
sudo chown -R $(whoami) ~/vcpkg
```

### Linux：缺少系统依赖

**问题**: 缺少头文件或库

```bash
sudo apt install -y build-essential cmake git ninja-build \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxi-dev libgl1-mesa-dev libglu1-mesa-dev
```

### Linux：vcpkg 依赖失败

**问题**: vcpkg 无法安装某些包

```bash
sudo apt install -y g++ gcc rsync
```

## CI/CD

GitHub Actions 工作流：`ci.yml`（门禁）、`build-package.yml`（三平台打包）、`deploy-server.yml`（部署）、`docs.yml` / `deploy-docs.yml`（文档站）、`build-agent.yml`、`release.yml`、`verify-platforms.yml`、`nightly.yml`。

### ci.yml 任务矩阵

| Job | 内容 |
|-----|------|
| C++ Windows ×2 | MSVC 构建 + ctest + 覆盖率（失败时回显 gtest 明细） |
| C++ Linux (full tests) ×2 | 全量 ctest（2000+ 例）+ 覆盖率 |
| C++ ubuntu-22.04 | 兼容性构建 |
| C++ macos-15-intel | 构建验证（continue-on-error） |
| Go Server ×3 | ubuntu / windows / macos，`go test -race ./...` |
| Platform Boundary Guard | `scripts/check_platform_boundary.sh`：公共层禁平台宏，allowlist 只减不增 |

### build-package.yml

Windows x64 / Linux x64 / macOS x64（intel）/ Android 四条打包腿，各含 C++ runtime、Go server、Tauri GUI（三平台 `pnpm tauri build`）与 install 脚本产物。
