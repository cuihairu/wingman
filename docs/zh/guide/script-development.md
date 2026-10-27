---
title: 脚本开发指南
---
> [English](../../guide/script-development.md) | **中文**


# 脚本开发指南

Wingman 脚本用 **Lua** 或 **Python** 编写，调用统一的 `wingman` 模块（screen / input / vision / trigger / event 等）驱动自动化。本文覆盖从第一个脚本到调试的完整流程。

## 语言选择

| | Lua | Python |
|---|---|---|
| 启动速度 | 极快 | 较慢（解释器冷启动） |
| 依赖 | 仅 Lua 5.5（sol2 绑定） | Python 3（vcpkg `python3` 端口，当前 3.12.x；pybind11 嵌入） |
| 适用 | 高频、低延迟、嵌入式场景 | 复杂逻辑、复用 Python 第三方库 |
| 编辑/调试 | VS Code + EmmyLua（推荐） | VS Code + Python 扩展 |

> 本地 GUI 的脚本**编辑统一在 VS Code**，GUI 只负责加载/运行/停止。远程 Dashboard 才有内置编辑器。

> **Python 引擎默认关闭**：官方发布的 runtime 包（release/nightly）只内置 Lua 引擎，直接运行 `.py` 脚本会提示引擎不存在。要使用 Python 需自行构建：CMake 加 `-DWINGMAN_ENABLE_PYTHON=ON`（vcpkg manifest `python` feature，会拉起 `python3` + `pybind11` 端口，勿混用系统 CPython）。该维度已纳入主 CI：`cpp-linux-python-tests` job 在 Linux 上带 Python 开关跑全量测试，`cpp-python` job 验证 Windows 链接链路。

## 第一个脚本

### Lua

`hello.lua`：

```lua
local wingman = require("wingman")

print("Hello Wingman")
local w, h = wingman.screen.getScreenWidth(), wingman.screen.getScreenHeight()
print(string.format("屏幕: %dx%d", w, h))
```

运行：

```bash
wingman-agent script hello.lua
```

### Python

`hello.py`：

```python
from wingman import screen, util

print("Hello Wingman")
w, h = screen.getScreenWidth(), screen.getScreenHeight()
print(f"屏幕: {w}x{h}")
```

运行（需启用 Python 引擎的构建，见上节「Python 引擎默认关闭」）：

```bash
wingman-agent script hello.py
```

> Python 侧的 `wingman` 模块面与 Lua 完全一致（引擎无关的统一注册），模块函数同时以原始 camelCase 与 snake_case 两个名字暴露——`screen.getScreenWidth()` 与 `screen.get_screen_width()` 等价，后者符合 PEP 8。`libs/python/typing/` 提供 `.pyi` 类型桩。

## 核心模块速览

完整 API 见侧栏「API」分类。常用模块：

| 模块 | 典型用途 | 代表函数 |
|------|----------|----------|
| `screen` | 像素/图像检测、截图 | `findColor`、`findImage`、`capture`、`getScreenWidth` |
| `input` | 鼠标键盘模拟（含人性化） | `click`、`move`、`key`、`type` |
| `vision` | 高级视觉（主色、模板匹配、AI 检测） | `getDominantColor`、`findImage` |
| `trigger` | 条件触发器 | 颜色出现/消失、图像、热键、定时 |
| `event` | 发布-订阅事件 | `on`、`emit` |
| `task` | 任务编排 | 创建/等待/取消 |
| `notify` | 通知（日志/toast） | `info`、`warn`、`error`、`toast` |
| `http` | HTTP 客户端 | `get`、`post`、`put`、`delete` |
| `human` | 人性化输入（防检测） | 随机延迟、轨迹移动 |
| `util` | 工具（时间、睡眠、日志） | `sleep`、`getTime`、`log` |
| `kv` / `db` | 持久化 | 键值存储、SQLite |

> 注意：Lua 统一经 `require("wingman")` 后以 `wingman.<module>.<fn>` 命名空间访问，没有裸全局模块；Python 统一从 `wingman` 包导入。以 [examples/lua_scripts/](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts) 的真实用法为准。

## 沙箱模式

本地两条执行路径——CLI `wingman-agent script` 与本地 GUI（Standalone 模式）——均**强制沙箱运行**（`apps/agent/src/commands/script_command.cpp` 与 `apps/agent/src/standalone_mode.cpp` 写死 `sandboxed = true`）。经 `wingman.script.load` API 加载时可经 `sandboxed` 配置另行指定（见 [script API](../api/script.md)）。两种引擎的沙箱面：

**Lua**：

- 移除全局 `io`、`os`、`debug`、`package` 与 `dofile` / `loadfile` / `load`——脚本内没有磁盘读取与系统调用路径。
- `require` 为受控 shim：仅 `require("wingman")` 解析到 wingman 模块表，其余模块名报错 `sandbox: module not allowed: <name>`。
- 常用替代：时间用 `wingman.util.getTime()`（epoch 毫秒）、文件用 `wingman.file.read` / `write` / `exists`、等待用 `wingman.util.sleep(ms)`。
- 无 `os.getenv`：脚本配置 `env` 注入的键以 `_ENV_<KEY>` 全局变量读取。

**Python**：

- `__builtins__` 替换为白名单（`print` / `len` / `range` 与基础类型、异常类等），并清掉 `os` / `sys` / `subprocess` / `io` / `importlib` 别名。
- `import` 由受控 shim 接管：仅 `import wingman`（含 `wingman.*` 子模块与 `from wingman import <module>`）放行，其余模块——包括标准库——报 `ImportError: sandbox: module not allowed: <name>`。因此不要 `import time`，等待用 `wingman.util.sleep(ms)`。
- 沙箱下 `env` 注入关闭（无 `os` 可读环境变量）。

## 典型模式

### 像素检测 + 点击（颜色触发）

```lua
local wingman = require("wingman")

-- findColor(color, region 表, tolerance) 返回单数组：res[1] = 命中点，res[2] = 是否找到
-- （绑定层只回传单个 table，Lua 侧不会展开成多返回值）
local res = wingman.screen.findColor(0xFF0000, {x = 0, y = 0, width = 1920, height = 1080}, 10)
if res[2] then
    wingman.input.click(res[1].x, res[1].y)
end
```

### 循环监控（带退出条件）

```lua
local wingman = require("wingman")

for i = 1, 100 do
    local res = wingman.screen.findColor(0x00FF00, {x = 0, y = 0, width = 1920, height = 1080}, 10)
    if res[2] then
        wingman.util.log("检测到目标: " .. res[1].x .. "," .. res[1].y)
        break
    end
    wingman.util.sleep(500)  -- 500ms 间隔
end
```

> 需要全部命中点时改用 `wingman.screen.findColors(color, region, tolerance, maxCount?)`，返回点数组。`window.find` 同 `findColor` 一样返回单数组 `{handle, found}`。

更多模式（图像匹配、热键触发、HTTP 联动、宏录制回放）见 [examples](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts) 与 [触发器指南](../guides/triggers.md)。

## 运行方式

| 模式 | 命令 | 适用 |
|------|------|------|
| 单脚本 | `wingman-agent script foo.lua` | 一次性任务、快速验证 |
| 本地 GUI | `wingman-agent start`（local 能力）+ Tauri GUI | 单机带界面日常使用 |
| 远程编排 | `wingman-agent start`（agent 能力）→ Go server → Dashboard | 多机集中管控 |

三种模式的控制路径与架构约束见 [快速开始 - 运行模式](./getting-started.md#运行模式) 与 [通信协议](../protocols.md)。

## 函数命名约定

模块函数在 C++ 描述符中以 camelCase 登记（如 `joinTeam`、`getEventCount`）。两种脚本面均可用：

- **Lua**：只有 camelCase 原名（`wingman.team.joinTeam(...)`）。
- **Python**：camelCase 原名与 snake_case 别名**同时暴露**（`wingman.team.join_team(...)` 与 `wingman.team.joinTeam(...)` 等价；别名规则与引擎内 `camelToSnake` 一致）。Python typing 桩（`libs/python/typing/wingman/*.pyi`）以 snake_case 为主形，docstring 标注对应 camelCase 形式。

少数函数直接以 snake_case 登记（如 `db.table_insert`、`human.mouse_move`），此时两种语言同名。脚本自定义的可手写 pythonic 别名不受此约束（如 `timer.after`）。

**漂移守卫**：`python3 scripts/audit_pyi_drift.py` 比对 C++ 模块描述符与 pyi 桩（模块面、函数面、`__init__` 导出面），漂移即非零退出；CI 中 `api-typing-audit` job 同口径把关。

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
