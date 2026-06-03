# Wingman 依赖安装指南

## 问题说明

由于网络限制，无法直接从 GitHub 克隆 vcpkg。本文档提供了几种安装依赖的方法。

## 方法 1：使用 vcpkg（推荐）

### 前提条件
- 网络连接
- Git
- Visual Studio 2022

### 步骤

1. **克隆 vcpkg**（在网络恢复后）
   ```cmd
   cd wingman
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   bootstrap-vcpkg.bat -disableMetrics
   ```

2. **安装依赖**（使用静态链接）
   ```cmd
   vcpkg install --triplet=x64-windows-static lua opencv4 spdlog nlohmann-json asio curl sqlite3 protobuf
   ```

3. **配置项目**
   ```cmd
   cd ..
   build-scripts\build-runtime-msvc-ninja.bat
   ```

## 方法 2：使用预构建依赖包

如果无法使用 vcpkg，可以从以下位置获取预构建的依赖：

| 依赖 | 预构建包位置 |
|------|-------------|
| Lua | https://sourceforge.net/projects/luabinaries/files/5.4.8/Tools%20Executables/ |
| OpenCV | https://opencv.org/releases/ |
| spdlog | https://github.com/gabime/spdlog/releases |
| nlohmann/json | https://github.com/nlohmann/json/releases |
| asio | https://sourceforge.net/projects/asio/files/asio/ |

## 方法 3：使用 Scoop（部分依赖）

已安装：Lua 5.4.8

```cmd
scoop install lua
```

## 当前状态

✅ 已安装：
- Lua 5.4.8 (via Scoop)

❌ 待安装：
- OpenCV (图像处理)
- spdlog (日志)
- nlohmann-json (JSON)
- asio (网络)
- curl (HTTP)
- sqlite3 (数据库)
- protobuf (序列化)

## 临时方案

可以使用 `build-scripts\configure_minimal.bat` 配置最小化构建，但这是备用路径，功能受限。

## 建议

1. 等待网络恢复后运行 `setup.bat`
2. 或者使用预构建的依赖包
3. 或者联系团队成员获取 vcpkg 安装包的副本

## 离线安装

如果有 vcpkg 安装包的副本：

1. 将 vcpkg 目录复制到项目根目录
2. 运行 `build-scripts\build-runtime-msvc-ninja.bat`
3. 构建项目
