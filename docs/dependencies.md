# Wingman 依赖说明

依赖统一由 vcpkg manifest 管理，清单是仓库根目录的 `vcpkg.json`（与 baseline 版本绑定）。CMake 经 toolchain 在 configure 阶段自动安装，不需要也不允许手工 `vcpkg install <包名>` 拼清单，不回退到系统库（见 CLAUDE.md 依赖管理规则）。

## 核心依赖（manifest，始终安装）

asio、curl、lua、nlohmann-json、opencv4（仅 Windows）、openssl、sol2、spdlog、sqlite3

其中 OpenCV 有平台门：`platform: "windows"`，Linux 走 `vision` feature，macOS 不带 OpenCV（视觉分析走内置逐像素路径）。

## 可选 feature（`-DVCPKG_MANIFEST_FEATURES=`）

| feature | 装什么 | 开关 |
|---------|--------|------|
| `tests` | gtest | `WINGMAN_BUILD_TESTS=ON` |
| `ocr` | tesseract（Windows） | `WINGMAN_ENABLE_OCR=ON` |
| `ml` | onnxruntime（Windows/Linux） | `WINGMAN_ENABLE_ML=ON` |
| `vision` | opencv4（Linux） | 自动：CMake 找到 OpenCV 即定义 `WINGMAN_ENABLE_VISION`（Linux 需此 feature 提供 OpenCV；Windows 基础依赖已含） |
| `python` | python3 + pybind11 | `WINGMAN_ENABLE_PYTHON=ON` |

Android 用独立的 `cpp/vcpkg-android.cmake` 叠加 NDK 工具链（见 apps/android/app/build.gradle.kts）。

## 配置命令

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=tests \
  -DWINGMAN_BUILD_TESTS=ON
```

Windows 快捷脚本：`build-scripts\configure-msvc-ninja.bat` + `build-scripts\build-runtime-msvc-ninja.bat`。完整步骤见 [BUILD.md](../BUILD.md)。

## 版本与 baseline

- baseline：`vcpkg.json` 的 `builtin-baseline`
- Windows 三元组：`x64-windows-static`（/MT 静态运行时）
- Linux 三元组：`x64-linux`；Android：`arm64-android`

## 子模块依赖（vcpkg 外，钉版 commit）

Crashpad 崩溃采集在 vcpkg 官方 registry 无 port，经批准以 git 子模块钉版接入（例外声明与设计口径见 [crash-reporting-design.md](design/crash-reporting-design.md)），构建期零网络。zlib 仍走 vcpkg manifest。

| 子模块 | 钉版 | 许可 | 用途 |
|--------|------|------|------|
| `third_party/crashpad-cmake` | `80573ad` | Apache-2.0 | CMake 包装：只取其 `cmake/` 模块文件直驱（`cmake/crashpad.cmake`），不经其 FetchContent 根 |
| `third_party/crashpad`（backtrace-labs fork） | `7b9686b` | Apache-2.0 | 崩溃采集本体（client 库 + handler 进程） |
| `third_party/mini_chromium` | `9cdc2a7` | BSD-3-Clause | crashpad 依赖的 chromium base 子集 |
| `third_party/lss`（cpp-pm 镜像，上游 chromium.googlesource.com/linux-syscall-support） | `e1e7b0a` | BSD-3-Clause | Linux 直接系统调用支持头 |

开关 `WINGMAN_ENABLE_CRASHPAD` 默认仅 Linux 开；升级走子模块指针更新。本机缺子模块时 Linux 配置期会明确报错（`git submodule update --init third_party/`）。

## 历史说明

本文档曾是网络受限时期的安装绕行指南（预构建包、Scoop、手工 `vcpkg install` 清单）。那些路径与 manifest 模式冲突，2026-10-04 已删除；依赖缺失时的做法是补 `vcpkg.json`、配好 toolchain 或执行 manifest 驱动的安装，不是换来源。
