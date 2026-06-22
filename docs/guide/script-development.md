---
title: 脚本开发指南
---

# 脚本开发指南

Wingman 脚本用 **Lua** 或 **Python** 编写，调用统一的 `wingman` 模块（screen / input / vision / trigger / event 等）驱动自动化。本文覆盖从第一个脚本到调试的完整流程。

## 语言选择

| | Lua | Python |
|---|---|---|
| 启动速度 | 极快 | 较慢（解释器冷启动） |
| 依赖 | 仅 Lua 5.4（sol2 绑定） | Python 3.8+（pybind11） |
| 适用 | 高频、低延迟、嵌入式场景 | 复杂逻辑、复用 Python 生态 |
| 编辑/调试 | VS Code + EmmyLua（推荐） | VS Code + Python 扩展 |

> 本地 GUI 的脚本**编辑统一在 VS Code**，GUI 只负责加载/运行/停止。远程 Dashboard 才有内置编辑器。

## 第一个脚本

### Lua

`hello.lua`：

```lua
local screen = require("wingman.screen")
local input  = require("wingman.input")
local util   = require("wingman.util")

print("Hello Wingman")
local w, h = screen.getScreenWidth(), screen.getScreenHeight()
print(string.format("屏幕: %dx%d", w, h))
```

运行：

```bash
wingman-runtime script hello.lua
```

### Python

`hello.py`：

```python
from wingman import screen, util

print("Hello Wingman")
w, h = screen.getScreenWidth(), screen.getScreenHeight()
print(f"屏幕: {w}x{h}")
```

运行：

```bash
wingman-runtime script hello.py
```

## 核心模块速览

完整 API 见侧栏「API」分类。常用模块：

| 模块 | 典型用途 | 代表函数 |
|------|----------|----------|
| `screen` | 像素/图像检测、截图 | `findColor`、`findImage`、`capture`、`getScreenWidth` |
| `input` | 鼠标键盘模拟（含人性化） | `click`、`move`、`keyPress`、`type` |
| `vision` | 高级视觉（主色、模板匹配） | `getDominantColor`、`matchTemplate` |
| `trigger` | 条件触发器 | 颜色出现/消失、图像、热键、定时 |
| `event` | 发布-订阅事件 | `on`、`emit` |
| `task` | 任务编排 | 创建/等待/取消 |
| `notify` | 通知（日志/toast） | `log`、`warn`、`error` |
| `http` | HTTP 服务端（脚本内） | 路由、响应 |
| `human` | 人性化输入（防检测） | 随机延迟、轨迹移动 |
| `util` | 工具（时间、睡眠） | `sleep`、`getTime` |
| `kv` / `db` | 持久化 | 键值存储、SQLite |

> 注意：部分模块在 Lua 中为全局（`screen` / `input`），部分需 `require`；Python 统一从 `wingman` 包导入。以 [examples/lua_scripts/](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts) 的真实用法为准。

## 典型模式

### 像素检测 + 点击（颜色触发）

```lua
local screen = require("wingman.screen")
local input  = require("wingman.input")

local found, x, y = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if found then
    input.click(x, y)
end
```

### 循环监控（带退出条件）

```lua
local screen = require("wingman.screen")
local util   = require("wingman.util")

for i = 1, 100 do
    local found, x, y = screen.findColor(0x00FF00, 0, 0, 1920, 1080, 10)
    if found then
        util.log("检测到目标: " .. x .. "," .. y)
        break
    end
    util.sleep(500)  -- 500ms 间隔
end
```

更多模式（图像匹配、热键触发、HTTP 联动、宏录制回放）见 [examples](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts) 与 [触发器指南](../guides/triggers.md)。

## 运行方式

| 模式 | 命令 | 适用 |
|------|------|------|
| 单脚本 | `wingman-runtime script foo.lua` | 一次性任务、快速验证 |
| 本地 GUI | `wingman-runtime start`（local 能力）+ Tauri GUI | 单机带界面日常使用 |
| 远程编排 | `wingman-runtime start`（agent 能力）→ Go server → Dashboard | 多机集中管控 |

三种模式的控制路径与架构约束见 [快速开始 - 运行模式](./getting-started.md#运行模式) 与 [通信协议](../protocols.md)。

## 调试（Lua / EmmyLua）

1. VS Code 安装 `tangzx.emmylua` 扩展（仓库 `.vscode/extensions.json` 已推荐）。
2. 补全：项目自带 [assets/vscode-wingman-lua/wingman.d.lua](https://github.com/cuihairu/wingman/tree/main/assets/vscode-wingman-lua)，提供 `wingman.*` 模块的类型提示。
3. 断点调试：runtime 监听 `:9966`（EmmyLua attach 模式）。VS Code `launch.json` 已配置，按 F5 attach。
4. 详见 [调试指南](./debugging.md) 与 [VS Code 开发环境](../development-environment.md)。

## 下一步

- [快速开始](./getting-started.md)
- [API 参考](../api/script.md)
- [示例脚本](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts)
- [通信协议](../protocols.md)
