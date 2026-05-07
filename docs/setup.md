# Wingman 开发环境设置

## 前置要求

- Windows 10/11
- Visual Studio 2022（Community/Enterprise/Professional）
- Git
- 网络连接（用于下载依赖）

## 快速开始

### 1. 安装依赖

运行提供的安装脚本：

```cmd
setup.bat
```

这将：
- 克隆 vcpkg 到项目目录
- 安装所有必需的依赖包
- 首次运行可能需要 30-60 分钟

### 2. 配置 CMake

```cmd
configure.bat
```

### 3. 构建

```cmd
cmake --build build --config Debug
```

或打开 `build\Wingman.sln` 在 Visual Studio 中开发。

## 手动设置

如果脚本无法运行，可以手动执行以下步骤：

### 安装 vcpkg

```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat -disableMetrics
```

### 安装依赖

```cmd
vcpkg install --triplet=x64-windows lua opencv4 spdlog nlohmann-json asio curl sqlite3 protobuf
```

### 配置 CMake

```cmd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake
```

## 项目结构

```
wingman/
├── client/             # Client 可执行文件
│   ├── src/           # 源文件
│   ├── include/       # 头文件
│   └── config/        # 配置文件
├── libs/              # 共享库
│   ├── proto/               # Protobuf 封装
│   ├── transport/           # 传输层
│   ├── core/                # 核心功能
│   ├── lua/                 # Lua 引擎
│   └── debug/               # 调试器适配器
├── protobuf/              # Protobuf 定义
├── server/             # Go Server
└── build/              # 构建输出
```

## 模块说明

### Client 模式

- **ActiveMode**: 主动连接服务器
- **PassiveMode**: 监听客户端连接
- **StandaloneMode**: 单机脚本执行

### 依赖包

| 包名 | 用途 |
|------|------|
| lua | Lua 脚本引擎 |
| opencv4 | 图像处理 |
| spdlog | 日志库 |
| nlohmann-json | JSON 解析 |
| asio | 网络库 |
| curl | HTTP 客户端 |
| sqlite3 | 数据库 |
| protobuf | 序列化协议 |

## 故障排除

### vcpkg 安装失败

- 检查网络连接
- 尝试使用 VPN 或代理
- 清理 `vcpkg` 目录后重试

### CMake 配置失败

- 确保已安装 Visual Studio 2022
- 确保已运行 `setup.bat`
- 检查 vcpkg 依赖是否正确安装

### 编译错误

- 确保使用 C++17 标准
- 检查所有依赖是否正确链接
- 清理 `build` 目录后重新配置
