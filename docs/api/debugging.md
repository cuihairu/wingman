# 调试 API

调试 API 提供脚本调试功能，支持 EmmyLua 调试器。

## 📋 目录

- [概述](#概述)
- [EmmyLua 调试器](#emmylua-调试器)
- [调试函数](#调试函数)
- [VS Code 配置](#vs-code-配置)
- [调试技巧](#调试技巧)

---

## 概述

Wingman 提供了强大的脚本调试功能，支持在 VS Code 中进行断点调试。

### 支持的调试器

| 调试器 | 脚本语言 | 状态 |
|--------|----------|------|
| EmmyLua | Lua | ✅ 支持 |
| debugpy | Python | 🚧 计划中 |

### 系统要求

- **IDE**: Visual Studio Code
- **VS Code 插件**: [EmmyLua](https://marketplace.visualstudio.com/items?itemName=tangzx.emmylua)
- **Wingman**: 已编译并启用调试支持

---

## EmmyLua 调试器

EmmyLua 是一个强大的 Lua 调试器，支持断点、变量查看、调用栈等功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

Python 调试暂未实现，计划使用 debugpy。

---

## 调试函数

### `start()`

启动调试器。

**示例:**
```lua
local wingman = require("wingman")

-- Lua
wingman.debugger.start()

print("Debugging started...")
```

### `stop()`

停止调试器。

**示例:**
```lua
-- Lua
debug.stop()
print("Debugging stopped")
```

### `breakpoint()`

设置断点。

**参数:** 无

**说明:** 当脚本执行到此函数时，会暂停执行，可以在 VS Code 中查看变量和调用栈。

**示例:**
```lua
-- Lua
local function calculate(a, b)
    local result = a + b
    debug.breakpoint()  -- 在此处暂停
    return result
end

calculate(10, 20)
```

### `log(message, ...)`

输出调试日志。

**参数:**
- `message` (string): 日志消息
- `...` (any): 额外参数（可选）

**示例:**
```lua
-- Lua
debug.log("Script started")
debug.log("Player position:", x, y)

-- 带格式化的日志
debug.log(string.format("Health: %d/%d", currentHealth, maxHealth))
```

### `watch(name, value)`

监视变量。

**参数:**
- `name` (string): 变量名
- `value` (any): 变量值

**说明:** 在 VS Code 的调试视图中显示被监视的变量。

**示例:**
```lua
-- Lua
local playerX = 100
local playerY = 200

debug.watch("playerX", playerX)
debug.watch("playerY", playerY)

-- 更新监视
playerX = 150
debug.watch("playerX", playerX)
```

### `evaluate(code)`

执行代码表达式。

**参数:**
- `code` (string): 要执行的代码

**返回:** 执行结果

**示例:**
```lua
-- Lua
local result = debug.evaluate("1 + 1")
print(result)  -- 2

-- 执行复杂表达式
local data = {x = 10, y = 20}
local sum = debug.evaluate("data.x + data.y")
print(sum)  -- 30
```

### `stacktrace()`

获取调用栈。

**返回:** 调用栈信息（数组）

**示例:**
```lua
-- Lua
local function level3()
    local stack = debug.stacktrace()
    for i, frame in ipairs(stack) do
        print(string.format("%d: %s at line %d",
            i, frame.function, frame.line))
    end
end

local function level2()
    level3()
end

local function level1()
    level2()
end

level1()
-- 输出:
-- 1: level3 at line 3
-- 2: level2 at line 10
-- 3: level1 at line 14
```

### `locals()`

获取当前局部变量。

**返回:** 局部变量表

**示例:**
```lua
-- Lua
local function test()
    local a = 10
    local b = 20
    local c = "hello"

    local vars = debug.locals()
    for name, value in pairs(vars) do
        print(name, "=", value)
    end
    -- 输出:
    -- a = 10
    -- b = 20
    -- c = hello
end

test()
```

### `globals()`

获取全局变量。

**返回:** 全局变量表

**示例:**
```lua
-- Lua
local allGlobals = debug.globals()

-- 获取特定全局变量
print(debug.globals().print)
```

### `isAttached()`

检查调试器是否已连接。

**返回:** boolean

**示例:**
```lua
-- Lua
if debug.isAttached() then
    print("Debugger is attached")
else
    print("No debugger attached")
end
```

---

## VS Code 配置

### 1. 安装 EmmyLua 插件

在 VS Code 中安装 [EmmyLua](https://marketplace.visualstudio.com/items?itemName=tangzx.emmylua) 插件。

### 2. 配置 launch.json

在 `.vscode/launch.json` 中添加调试配置：

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "type": "emmylua_new",
            "request": "launch",
            "name": "Attach to Wingman Lua",
            "host": "localhost",
            "port": 9966,
            "ideConnectDebugger": true,
            "cwd": "${workspaceFolder}",
            "ext": [
                ".lua",
                ".lua.txt",
                ".lua.bytes"
            ]
        }
    ]
}
```

### 3. 启动调试

有两种方式启动调试：

#### 方式一：从 VS Code 启动

1. 在 VS Code 中打开 Lua 脚本
2. 在代码行号左侧点击设置断点（红点）
3. 按 F5 或点击"运行和调试"面板
4. 选择 "Attach to Wingman Lua" 配置
5. 脚本会在断点处暂停

#### 方式二：从脚本启动

在脚本开头添加：

```lua
local wingman = require("wingman")

wingman.debugger.start()

-- 你的脚本代码
print("Hello, World!")

-- 设置断点
wingman.debugger.breakpoint()

print("This line will be hit after debugger attaches")
```

然后在 VS Code 中启动调试。

### 4. 调试操作

在 VS Code 调试模式下，你可以：

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| 继续 | F5 | 继续执行到下一个断点 |
| 单步跳过 | F10 | 执行当前行，不进入函数 |
| 单步进入 | F11 | 进入函数内部 |
| 单步跳出 | Shift+F11 | 跳出当前函数 |
| 重启 | Ctrl+Shift+F5 | 重新启动调试 |
| 停止 | Shift+F5 | 停止调试 |

### 5. 查看变量

在调试时，你可以：

- **变量面板**: 查看当前作用域的所有变量
- **监视面板**: 添加要监视的表达式
- **调用堆栈**: 查看函数调用链
- **调试控制台**: 执行 Lua 表达式

---

## 调试技巧

### 1. 条件断点

在 VS Code 中可以设置条件断点，只在满足条件时才暂停。

```lua
-- 右键点击断点，选择"编辑断点"
-- 添加条件: i == 10
for i = 1, 100 do
    print(i)
    -- 只有当 i == 10 时才会在此处暂停
end
```

### 2. 日志断点

使用日志断点而不暂停执行。

```lua
-- 在断点上右键，选择"编辑断点"
-- 选择"日志消息"
-- 输入: "Current value: {i}"
for i = 1, 100 do
    print(i)
    -- 会在调试控制台输出日志，但不暂停
end
```

### 3. 表达式求值

在调试控制台中执行表达式：

```lua
-- 在调试控制台中输入：
-- 查看变量
playerX

-- 执行表达式
playerX + 100

-- 调用函数
string.len("hello")

-- 修改变量
playerX = 200
```

### 4. 错误处理

调试时捕获错误：

```lua
local ok, err = pcall(function()
    -- 可能出错的代码
    local result = someFunction()
    return result
end)

if not ok then
    debug.log("Error occurred:", err)
    debug.breakpoint()  -- 在错误处暂停
end
```

### 5. 性能分析

使用调试器分析性能：

```lua
local startTime = os.time()

-- 你的代码
for i = 1, 1000 do
    processData(i)
end

local endTime = os.time()
debug.log(string.format("Execution time: %d seconds", endTime - startTime))
```

### 6. 状态检查

检查当前游戏状态：

```lua
-- 检查玩家状态
debug.breakpoint()
debug.log("Player state:")
debug.watch("health", player.health)
debug.watch("mana", player.mana)
debug.watch("position", player.x, player.y)

-- 检查敌人状态
for i, enemy in ipairs(enemies) do
    debug.log(string.format("Enemy %d: HP=%d", i, enemy.hp))
end
```

### 7. 数据验证

验证复杂数据结构：

```lua
local data = {
    players = {
        {id = 1, name = "Player1", score = 100},
        {id = 2, name = "Player2", score = 200}
    },
    settings = {
        difficulty = "hard",
        mode = "multiplayer"
    }
}

-- 在断点处检查
debug.breakpoint()

-- 在调试控制台中：
-- data.players[1].name
-- data.settings.difficulty
-- #data.players
```

---

## 常见问题

### 调试器无法连接

**问题**: VS Code 显示"无法连接到调试器"

**解决方案**:
1. 确保 Wingman 运行时已启动调试支持
2. 检查 EmmyLua 插件是否正确安装
3. 确认 `launch.json` 配置正确
4. 尝试从脚本内部调用 `debug.start()`

### 断点不起作用

**问题**: 设置的断点没有被触发

**解决方案**:
1. 确保调试器已连接（`debug.isAttached()` 返回 true）
2. 检查断点位置是否在可执行的代码行上
3. 尝试使用 `debug.breakpoint()` 手动设置断点

### 变量显示不正确

**问题**: 变量面板中显示的值不正确或为 nil

**解决方案**:
1. 确保变量在当前作用域内
2. 尝试使用 `debug.locals()` 查看所有局部变量
3. 在调试控制台中直接访问变量

---

## 🔗 相关文档

- [核心 API](core.md)
- [脚本 API](script.md)
- [调试器](debugger.md)
- [快速开始](../getting-started.md)

---

**返回**: [API 概览](overview.md) | [主页](../README.md)
