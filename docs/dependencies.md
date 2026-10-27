> [中文](zh/dependencies.md) | **English**

# Wingman Dependencies

Dependencies are managed uniformly through the vcpkg manifest; the manifest is `vcpkg.json` at the repository root (pinned to a baseline version). CMake installs them automatically via the toolchain at configure time. Manually assembling a manifest with `vcpkg install <package>` is neither needed nor allowed, and falling back to system libraries is prohibited (see the dependency management rules in CLAUDE.md).

## Core dependencies (manifest, always installed)

asio, curl, lua, nlohmann-json, opencv4 (Windows only), openssl, sol2, spdlog, sqlite3, zlib (linked by crashpad_util; see "Submodule dependencies" below)

OpenCV has a platform gate: `platform: "windows"` (already included in the Windows base dependencies), Linux goes through the `vision` feature; macOS does not ship OpenCV — in this case the vision implementation switches to `src/vision_stub.cpp` (`lib/wingman/CMakeLists.txt:176-187` selects the source file based on `OpenCV_FOUND AND TARGET opencv_core`, and `WINGMAN_ENABLE_VISION` is only defined when matched). Vision functions such as `findImage` / `findColor` / `findAllColors` always return not-found or empty values (the stub implements them all as `std::nullopt` / `{false, ...}` / `{}`), so vision-based automation is unavailable on macOS.

## Optional features (`-DVCPKG_MANIFEST_FEATURES=`)

| feature | What it installs | Toggle |
|---------|--------|------|
| `tests` | gtest | `WINGMAN_BUILD_TESTS=ON` |
| `ocr` | tesseract (Windows) | `WINGMAN_ENABLE_OCR=ON` |
| `ml` | onnxruntime (Windows/Linux) | `WINGMAN_ENABLE_ML=ON` |
| `vision` | opencv4 (Linux) | Automatic: CMake defines `WINGMAN_ENABLE_VISION` as soon as it finds OpenCV (Linux needs this feature to provide OpenCV; the Windows base dependencies already include it) |
| `python` | python3 + pybind11 | `WINGMAN_ENABLE_PYTHON=ON` |

The "What it installs" and "Toggle" columns **must both be provided**: `-DVCPKG_MANIFEST_FEATURES=<feature>` only decides whether vcpkg installs the package, while the CMake option only decides which code path this repository takes; CMake does not set this variable automatically (only `.github/workflows/ci.yml` in the repo sets it; see "Configuration commands"). The consequences of a missing feature vary by module: `ml` fails hard at configure time (`find_package(onnxruntime CONFIG REQUIRED)`; the CMake comment itself states "report a missing dependency explicitly, do not silently degrade to a stub"); `ocr` fails at compile time due to missing tesseract headers; **only `vision` silently degrades** — `src/vision_stub.cpp` takes over, compilation succeeds but all vision functions return not-found/empty values, and the log contains only a single `STATUS "Vision support disabled"`.

Android uses the standalone `apps/android/cpp/vcpkg-android.cmake` to layer on the NDK toolchain (see apps/android/app/build.gradle.kts).

## Configuration commands

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=tests \
  -DWINGMAN_BUILD_TESTS=ON
```

Windows shortcut scripts: `build-scripts\configure-msvc-ninja.bat` + `build-scripts\build-runtime-msvc-ninja.bat`. Full steps are in [BUILD.md](../BUILD.md).

## Versions and baseline

- baseline: the `builtin-baseline` in `vcpkg.json`
- Windows triplet: `x64-windows-static` (/MT static runtime)
- Linux triplet: `x64-linux`; Android: `arm64-android`

## Submodule dependencies (outside vcpkg, pinned commits)

Crashpad crash collection has no port in the official vcpkg registry; it was approved for integration as a pinned git submodule (see [crash-reporting-design.md](design/crash-reporting-design.md) for the exception statement and design rationale), with zero network access at build time. zlib still goes through the vcpkg manifest.

| Submodule | Pinned version | License | Purpose |
|--------|------|------|------|
| `third_party/crashpad-cmake` | `80573ad` | Apache-2.0 | CMake wrapper: only its `cmake/` module files are used directly (`cmake/crashpad.cmake`), without going through its FetchContent root |
| `third_party/crashpad` (backtrace-labs fork) | `7b9686b` | Apache-2.0 | Crash collection body (client library + handler process) |
| `third_party/mini_chromium` | `9cdc2a7` | BSD-3-Clause | Chromium base subset required by crashpad |
| `third_party/lss` (cpp-pm mirror, upstream chromium.googlesource.com/linux-syscall-support) | `e1e7b0a` | BSD-3-Clause | Linux direct syscall support headers |

The `WINGMAN_ENABLE_CRASHPAD` toggle is enabled by default on Linux only; upgrades go through submodule pointer updates. When submodules are missing locally, Linux configure time fails with an explicit error (`git submodule update --init third_party/`).

## Historical note

This document used to be an installation workaround guide from the network-restricted period (prebuilt packages, Scoop, manual `vcpkg install` manifests). Those paths conflict with the manifest model and were removed on 2026-10-04; the correct response to missing dependencies is to complete `vcpkg.json`, configure the toolchain properly, or run manifest-driven installation — not to switch sources.
