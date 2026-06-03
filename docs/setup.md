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
build-scripts\configure-msvc-ninja.bat
```

### 3. 构建

```cmd
build-scripts\build-runtime-msvc-ninja.bat
```

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
vcpkg install --triplet=x64-windows-static lua opencv4 spdlog nlohmann-json asio curl sqlite3 protobuf
```

### 配置 CMake

```cmd
build-scripts\configure-msvc-ninja.bat
build-scripts\build-runtime-msvc-ninja.bat
```

## 项目结构

```
wingman/
├── apps/                    # 应用程序
│   ├── runtime/             # CLI 运行时
│   ├── gui/                 # Tauri GUI 应用
│   └── inspector/           # 图形化检查工具
├── lib/                     # 核心库
│   └── wingman/             # 核心功能库
├── libs/                    # 辅助库
│   ├── clasp/               # 命令行库
│   ├── debug/               # EmmyLua 调试器适配
│   ├── lua/                 # Lua 引擎绑定
│   ├── python/              # Python 引擎绑定
│   ├── proto/               # Protobuf 协议封装
│   └── transport/           # TCP/WebSocket 传输层
├── orchestrator/            # 编排层
│   ├── dashboard/           # Web 控制面板
│   └── server/             # Go 服务端
├── protobuf/                # Protobuf 协议定义
├── examples/                # 示例和模板
└── build/                   # 构建输出
```

详细说明请参考 [项目结构文档](./project-structure.md)。

## 运行模式

### Client 模式

- **Remote Mode**：主动连接 Go 编排器
- **StandaloneMode**：单机脚本执行

### 启动应用

```cmd
# CLI 运行时
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe

# GUI 应用
.\build\apps\gui\Release\wingman-gui.exe
```

## 依赖包

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

- 确保使用 C++23 标准
- 检查所有依赖是否正确链接
- 清理 `build` 目录后重新配置
