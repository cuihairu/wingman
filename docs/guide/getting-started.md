# 快速开始

## 环境要求

### 开发环境

- **Windows 10/11** - 主要支持平台
- **Visual Studio 2022** - C++17 支持
- **CMake 3.20+** - 构建系统
- **Git** - 版本控制
- **vcpkg** - C++ 包管理器

### vcpkg 安装

```bash
# 1. 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 2. 运行引导脚本
.\bootstrap-vcpkg.bat

# 3. 集成到系统（自动关联到 Visual Studio）
.\vcpkg integrate install

# 4. 设置环境变量（可选，用于自动 triplet 检测）
set VCPKG_ROOT=C:\path\to\vcpkg
```

## 项目依赖

Wingman 使用以下 vcpkg 包：

| 包名 | 用途 |
|-----|------|
| `lua` | LuaJIT 脚本引擎 |
| `opencv4` | 图像处理（可选） |
| `spdlog` | 日志库 |
| `nlohmann-json` | JSON 配置解析 |
| `asio` | 网络库 |
| `sol2` | C++/Lua 绑定（header-only） |

## 编译项目

### 1. 克隆仓库

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman
```

### 2. 安装依赖

```bash
# 安装所有依赖
vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio

# 或使用 vcpkg.json 自动安装
vcpkg install
```

### 3. 使用 CMake 生成项目

```bash
# 指定 vcpkg 工具链
cmake -B build -S . -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# 或设置环境变量后简化命令
set CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake -B build -S . -G "Visual Studio 17 2022"
```

### 4. 编译

```bash
# 编译 Release 版本
cmake --build build --config Release

# 或打开 Visual Studio
start build/Wingman.sln
```

### 5. 运行

```bash
# 运行示例脚本
.\build\Release\wingman.exe scripts\examples\hello.lua
```

## vcpkg.json

项目根目录包含 `vcpkg.json`，声明依赖：

```json
{
  "dependencies": [
    {
      "name": "lua",
      "features": ["tool"]
    },
    "opencv4",
    {
      "name": "spdlog",
      "features": ["wchar"]
    },
    "nlohmann-json",
    "asio"
  ]
}
```

## 命令行参数

```bash
# 本地模式 - 执行脚本
wingman.exe script.lua

# Server 模式 - 被动监听
wingman.exe --server --port 8888

# Client 模式 - 主动连接
wingman.exe --connect nebula.server.com:9999

# 调试模式
wingman.exe --debug script.lua
wingman.exe --debug --listen 9999 script.lua

# 帮助信息
wingman.exe --help
```

## 第一个脚本

创建文件 `hello.lua`：

```lua
local wingman = require("wingman")

-- 打印问候
print("Hello from Wingman!")

-- 获取屏幕尺寸
local screen = require("wingman.screen")
local width, height = screen.getSize()
print(string.format("Screen size: %dx%d", width, height))

-- 延迟 1 秒
local util = require("wingman.util")
util.sleep(1000)

print("Script completed!")
```

运行：

```bash
wingman.exe hello.lua
```

## 开发容器（可选）

使用 Dev Container 在 VS Code 中开发：

```bash
# 在容器中打开
code .
```

## 下一步

- [API 参考](/api/) - 查看完整 API 文档
- [示例脚本](/examples/) - 学习更多用法
- [调试指南](/guide/debugging) - 如何调试脚本
- [远程控制](/guide/remote) - 使用 TCP 远程控制
