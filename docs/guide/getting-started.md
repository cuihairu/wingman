---
title: 快速开始
---

# 快速开始

## 环境要求

### 开发环境

- **Windows 10/11** - 主要支持平台
- **Visual Studio 2022** - C++17 支持
- **CMake 3.20+** - 构建系统
- **Git** - 版本控制
- **vcpkg** - C++ 包管理器

### Python 环境（可选）

如需使用 Python 脚本：
- **Python 3.8+**
- **pybind11**

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
| `lua` | Lua 脚本引擎 |
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
build-scripts\configure-msvc-ninja.bat
```

### 4. 编译

```bash
build-scripts\build-runtime-msvc-ninja.bat
```

### 5. 运行

```bash
# 运行 Lua 示例脚本
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe script scripts\examples\hello.lua

# 运行 Python 示例脚本
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe script scripts\examples\hello.py
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
# 执行脚本
wingman-runtime.exe script script.lua
wingman-runtime.exe script script.py

# 启动 Agent（读取 agent.toml，主动 outbound 连接 Go orchestrator）
wingman-runtime.exe start

# 打包单文件脚本运行时
wingman-runtime.exe build --script script.lua --output script-runtime.exe

# 加密打包（脚本源码以 AES-256-GCM 密文嵌入 PE 资源）
wingman-runtime.exe build --script script.lua --output script-runtime.exe --password 'your-passphrase'

# 帮助信息
wingman-runtime.exe --help
```

### 加密打包与加载

`build` 的加密相关选项：

| 选项 | 作用 |
|------|------|
| `--encrypt` | 打开加密（需要口令） |
| `--password <p>` | 指定口令，并隐含 `--encrypt` |
| `--no-encrypt` | 显式关闭加密（旧命令行兼容；写在 `--password` 之后即取消加密） |
| `--no-compress` | 关闭压缩 |

- 口令也可以不进命令行：`WINGMAN_PACK_PASSWORD=... wingman-runtime build --script ... --output ...`（命令行参数会留在 shell 历史与进程列表里，环境变量是更合适的默认来源）。
- 没有口令就不允许加密：产物打不开等于永久作废，故 `--encrypt` 无口令时打包直接失败，而不是产出一个打不开的 exe。
- 密钥由口令派生（PBKDF2-HMAC-SHA256，100000 轮迭代，随机 16 字节 salt 与 12 字节 IV 写进打包头），载荷为 AES-256-GCM 认证密文。改一个字节、换口令、或头部被篡改都会在加载时失败并给出可区分的原因（`Incorrect password` / `authentication failed` / `Hash verification failed`）。
- 运行加密产物时从环境变量取口令：

```bash
WINGMAN_SCRIPT_PASSWORD='your-passphrase' ./script-runtime.exe
```

  口令错误或缺失时 runtime 会记录失败原因并退回普通 GUI/Agent 模式，不会执行脚本。
- 兼容性：未加密包（新旧版本）都可被无口令加载；历史上被禁用的 v1 加密包（一次性随机密钥、只留下 `sha256(key)`）无法恢复，加载时明确拒绝并要求用 `--encrypt --password` 重新打包。

## 运行模式

Wingman 有三种运行模式，按控制路径区分（详见 [架构决策](../architecture-decisions.md)）：

| 模式 | 命令 | 控制路径 | 适用场景 |
|------|------|----------|----------|
| **单脚本** | `wingman-runtime script foo.lua` | 直接执行后退出 | 一次性任务、CI、快速验证 |
| **本地 GUI** | `wingman-runtime start`（local 能力）+ Tauri GUI | `Tauri UI → local IPC（Named Pipe/UDS）→ runtime` | 单机有界面的日常使用 |
| **远程编排** | `wingman-runtime start`（agent 能力）→ Go orchestrator | `runtime agent → outbound → Go server → Dashboard` | 多机集中管控、团队协作 |

- 本地 GUI 与远程编排可并存：runtime 同时承载 local IPC 服务和 agent outbound 连接。
- **架构硬约束**：runtime 不开 HTTP/WebSocket server 作为控制面；Dashboard 只连 Go server，不直连 runtime。
- Go orchestrator 与 Dashboard 的部署见 [`orchestrator/server/README.md`](../../orchestrator/server/README.md)。

## 第一个脚本

### Python 版本

创建文件 `hello.py`：

```python
from wingman import screen, util

# 打印问候
print("Hello from Wingman!")

# 获取屏幕尺寸
width, height = screen.get_size()
print(f"Screen size: {width}x{height}")

# 延迟 1 秒
util.sleep(1000)

print("Script completed!")
```

### Lua 版本

创建文件 `hello.lua`：

```lua
local wingman = require("wingman")

-- 打印问候
print("Hello from Wingman!")

-- 获取屏幕尺寸
local width, height = wingman.screen.getSize()
print(string.format("Screen size: %dx%d", width, height))

-- 延迟 1 秒
wingman.util.sleep(1000)

print("Script completed!")
```

运行：

```bash
# Python
wingman-runtime.exe script hello.py

# Lua
wingman-runtime.exe script hello.lua
```

## 选择脚本语言

Wingman 同时支持 Python 和 Lua，如何选择？

### 使用 Python 如果你：
- 熟悉 Python 语法
- 需要使用丰富的 Python 库
- 需要更好的 IDE 支持（类型提示、自动补全）
- 开发复杂的项目

### 使用 Lua 如果你：
- 需要最快的启动速度
- 内存受限的环境
- 偏好简洁的语法

## 开发容器（可选）

使用 Dev Container 在 VS Code 中开发：

```bash
# 在容器中打开
code .
```

## 下一步

- [API 参考](../api/index.md) - 查看完整 API 文档
- [示例脚本](../examples/index.md) - 学习更多用法
- [调试指南](debugging.md) - 如何调试脚本
- [远程协议](../remote_protocol.md) - 了解远程控制协议
- [YOLO 模型使用](../guides/yolo-guide.md) - 准备 ONNX 模型并验证加载
