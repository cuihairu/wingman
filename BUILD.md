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
.\build-msvc-ninja-vcpkg\apps\agent\wingman-agent.exe
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
cmake --build build-tests --config Debug --target core_tests agent_tests transport_tests
ctest --test-dir build-tests -C Debug --output-on-failure
```

`WINGMAN_BUILD_TESTS=ON` 会自动启用标准 C++ 测试目标（core/agentcore/runtime/transport；启用 Python 引擎时含 python 绑定测试）。
如果还需要单独验证 Lua 绑定层，再额外加 `-DBUILD_LUA_TESTS=ON` 并构建 `lua_tests`。
建议测试使用单独的 `build-tests/` 目录，避免和现有 `build/` 的生成器或配置冲突。

## 性能基准测试

### 编译基准测试

```bash
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake -S . -B build-msvc-ninja-vcpkg -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:\Users\admin\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release -DWINGMAN_BUILD_BENCHMARKS=ON"
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake --build build-msvc-ninja-vcpkg --target kvstore_bench"
```

> Ninja 是单配置生成器，`--build --config` 会被静默忽略；构建类型只由
> `CMAKE_BUILD_TYPE` 决定，**必须显式给 Release**，否则基准测的是无优化读数
> （`scripts/run-benchmarks.sh` 亦显式设 Release）。改用 MSVC 多配置工程时
> 则换成 `-G "Visual Studio 17 2022"` + `--config Release`。

### 运行基准测试

```bash
.\build-msvc-ninja-vcpkg\lib\wingman\tests\benchmarks\kvstore_bench.exe
```

### 性能指标

下表是一次历史快照（无日期与机器标注），**仅示意量级**：绝对值受宿主负载影响大，
kvstore 口径是「1 次热身 + 单轮均值 + µs」，其中 DELETE 实为 set+del、LPOP 实为
lpop+lpush 的复合操作。带日期的基线、其余四套基准（event_hub / lua / vision /
matcher_bench）与计时方法学以 `docs/benchmarks.md` 为准。

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
| `WINGMAN_ENABLE_CRASHPAD` | Linux ON / 其他 OFF | 崩溃采集（Windows/macOS 构建代码就位但默认关，见 `docs/dependencies.md`） |
| `WINGMAN_COMPAT_BUILD` | OFF | 只构建可移植兼容目标 |
| `WINGMAN_BUILD_AGENT` | ON | 构建 Agent 多模块（同时决定 TRANSPORT/CORE/LUA/DESKTOP 默认值） |
| `WINGMAN_BUILD_TRANSPORT` / `CORE` / `LUA` / `DESKTOP` | 跟随 AGENT | 各模块单独开关（DESKTOP = 桌面 agent 应用，`CMakeLists.txt` 无 `WINGMAN_BUILD_RUNTIME` 选项） |
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
- asio、curl、lua、nlohmann-json、opencv4（仅 Windows；Linux 走 `vision` feature，macOS 无）、openssl、sol2、spdlog、sqlite3

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

GitHub Actions 工作流：`ci.yml`（门禁）、`build-package.yml`（四条打包腿：Windows x64 / Linux x64 / macOS x64 / Android arm64）、`deploy-server.yml`（部署）、`docs.yml` / `deploy-docs.yml`（文档站）、`build-agent.yml`（agent 单二进制 6 腿矩阵）、`release.yml`、`verify-platforms.yml`、`nightly.yml`。

### ci.yml 任务矩阵

ci.yml 共 9 个 job：`cpp-compat` 展开 2 个矩阵条目、`go-build` 展开 3 个，其余为单配置单腿。

| Job | 内容 |
|-----|------|
| Platform Boundary Guard | `scripts/check_platform_boundary.sh`：公共层禁平台宏，allowlist 只减不增 |
| Vault Touchpoint Scan | `scripts/check_vault_touchpoints.sh`：boot-tier 允许清单外明文凭证环境变量读取扫描 |
| API Typing Drift Audit | `scripts/audit_pyi_drift.py`（pyi stub 对 C++ 模块描述符）+ 重生成 Lua hints 并校验同步 |
| C++ Windows | MSVC Debug 构建 + ctest + **覆盖率**（OpenCppCoverage 采集，全 ci.yml 唯一覆盖率腿；失败时回显 gtest 明细） |
| C++ Windows (Python engine) | Windows 上 Python 引擎实验编译 + best-effort 测试 |
| C++ Linux (full tests) | ubuntu-24.04 全量 ctest（2000+ 例），xvfb-run 虚拟显示，**不采集覆盖率** |
| C++ Linux (full tests, Python engine) | 同上 + `WINGMAN_ENABLE_PYTHON=ON` 全量跑 |
| C++ ubuntu-22.04 | compat matrix，仅编译 `wingman_transport` 单 target |
| C++ macos-15-intel | compat matrix，同上且 `continue-on-error` |
| Go Server（3 OS matrix） | ubuntu / windows / macos，`go test -race ./...` |

### build-package.yml

Windows x64 / Linux x64 / macOS x64（intel）/ Android 四条打包腿，各含 C++ runtime、Go server、Tauri GUI（三平台 `pnpm tauri build`）与 install 脚本产物。
