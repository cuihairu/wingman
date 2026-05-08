# Wingman 项目说明

## 构建配置

**重要**: 此项目使用 vcpkg 静态库 (x64-windows-static) 进行构建。

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
