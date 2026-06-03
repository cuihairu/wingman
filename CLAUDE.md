# Wingman 项目说明

## 构建配置

**重要**: 此项目使用 vcpkg 静态库 (x64-windows-static) 进行构建。

### 依赖管理规则

1. 所有第三方依赖必须统一由 `vcpkg` 管理，版本以 `vcpkg.json` 和对应 baseline 为准。
2. 禁止在 CMake 中回退到系统环境依赖，例如 `/usr/local`、`/opt/homebrew`、系统 SDK、自带包管理器安装的库等。
3. 禁止使用本地头文件探测、手工 include 目录兜底、`FetchContent` 临时拉取来绕过 `vcpkg`。
4. 如果依赖缺失，应补齐 `vcpkg.json`、正确配置 `CMAKE_TOOLCHAIN_FILE`，或执行 `vcpkg install`，而不是改为依赖系统版本。
5. 如果某个模块依赖未在 `vcpkg` 中安装，应该明确报错或在设计上关闭该模块，不能静默切换到其他来源。
6. 命令行入口代码要保持可读性：参数分发可以集中，但每个具体命令实现应拆到独立文件，避免将多条命令逻辑堆在同一个源文件里。

### CMake 配置命令

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/Users/admin/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

### 编译命令

```bash
cmake --build build --config Release
```

## 项目架构

采用 apps + lib 架构，详见 ROADMAP.md。
