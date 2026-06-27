# API: wingman.smarttrigger

智能触发器模块，提供事件驱动的自动化能力。

## 模块概述

smarttrigger 模块提供智能触发器功能：
- **创建触发器** - 创建命名触发器
- **添加条件** - 颜色/图像/OCR 等检测条件
- **添加动作** - 点击/按键/等待/日志/脚本等动作
- **启动停止** - 启动或停止触发器
- **状态查询** - 查询运行状态、触发次数
- **参数配置** - 检查间隔

---

## 创建触发器

### create(name) / create(name)

**说明**：创建一个新的触发器。

**函数签名**：

```python
create(name: str) -> bool
```

```lua
create(name: string) -> boolean
```

**参数**：
- `name` - 触发器名称

**返回**：
- 是否创建成功

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

ok = smarttrigger.create("my_trigger")
if not ok:
    print("创建失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.smarttrigger.create("my_trigger")
if not ok then
    print("创建失败")
end
```

:::

---

## 添加触发条件

### addCondition(triggerName, condition) / addCondition(triggerName, condition)

**说明**：为指定触发器添加一个触发条件。条件以**对象表**形式传入。

**函数签名**：

```python
addCondition(triggerName: str, condition: dict) -> bool
```

```lua
addCondition(triggerName: string, condition: table) -> boolean
```

**参数**：
- `triggerName` - 触发器名称
- `condition` - 条件对象表，结构如下：

| 字段 | 类型 | 是否必填 | 说明 |
|------|------|---------|------|
| `type` | str | 是 | 条件类型，见下方「条件类型参考」 |
| `color` | `{r,g,b}` | 否 | 目标颜色（COLOR_* 类型使用） |
| `tolerance` | int | 否 | 颜色容差 |
| `threshold` | float | 否 | 匹配阈值（默认 0.8，IMAGE_* 类型使用） |
| `region` | `{x,y,width,height}` | 否 | 检测区域 |
| `text` | str | 否 | 目标文字（TEXT_* / OCR_* 类型使用） |
| `template` | str | 否 | 模板图像路径（IMAGE_* 类型使用） |

**返回**：
- 是否添加成功（触发器不存在或参数不足时返回 `false`）

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

# 颜色检测条件
smarttrigger.addCondition("my_trigger", {
    "type": "COLOR_FOUND",
    "color": {"r": 255, "g": 0, "b": 0},
    "tolerance": 10,
    "region": {"x": 100, "y": 100, "width": 50, "height": 50}
})

# 图像检测条件
smarttrigger.addCondition("my_trigger", {
    "type": "IMAGE_FOUND",
    "template": "target.png",
    "threshold": 0.8,
    "region": {"x": 0, "y": 0, "width": 800, "height": 600}
})

# OCR 条件
smarttrigger.addCondition("my_trigger", {
    "type": "OCR_CONTAINS",
    "text": "敌人",
    "region": {"x": 0, "y": 0, "width": 200, "height": 50}
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 颜色检测条件
wingman.smarttrigger.addCondition("my_trigger", {
    type = "COLOR_FOUND",
    color = {r = 255, g = 0, b = 0},
    tolerance = 10,
    region = {x = 100, y = 100, width = 50, height = 50}
})

-- 图像检测条件
wingman.smarttrigger.addCondition("my_trigger", {
    type = "IMAGE_FOUND",
    template = "target.png",
    threshold = 0.8,
    region = {x = 0, y = 0, width = 800, height = 600}
})

-- OCR 条件
wingman.smarttrigger.addCondition("my_trigger", {
    type = "OCR_CONTAINS",
    text = "敌人",
    region = {x = 0, y = 0, width = 200, height = 50}
})
```

:::

---

## 添加触发动作

### addAction(triggerName, action) / addAction(triggerName, action)

**说明**：为指定触发器添加一个触发动作。动作以**对象表**形式传入。

**函数签名**：

```python
addAction(triggerName: str, action: dict) -> bool
```

```lua
addAction(triggerName: string, action: table) -> boolean
```

**参数**：
- `triggerName` - 触发器名称
- `action` - 动作对象表，结构如下：

| 字段 | 类型 | 是否必填 | 说明 |
|------|------|---------|------|
| `type` | str | 是 | 动作类型，见下方「动作类型参考」 |
| `x` | int | 否 | 点击 X 坐标（CLICK 使用） |
| `y` | int | 否 | 点击 Y 坐标（CLICK 使用） |
| `key` | int | 否 | 按键码（KEY 使用） |
| `waitMs` | int | 否 | 等待毫秒数（WAIT 使用） |
| `script` | str | 否 | Lua 脚本内容（LUA_SCRIPT 使用） |
| `message` | str | 否 | 日志内容（LOG 使用） |

**返回**：
- 是否添加成功（触发器不存在或参数不足时返回 `false`）

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

# 点击动作
smarttrigger.addAction("my_trigger", {"type": "CLICK", "x": 100, "y": 200})

# 按键动作
smarttrigger.addAction("my_trigger", {"type": "KEY_PRESS", "key": 49})

# 等待动作
smarttrigger.addAction("my_trigger", {"type": "WAIT", "waitMs": 500})

# 日志动作
smarttrigger.addAction("my_trigger", {"type": "LOG", "message": "触发器被激活！"})

# Lua 脚本动作
smarttrigger.addAction("my_trigger", {"type": "LUA_SCRIPT", "script": "print('hi')"})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 点击动作
wingman.smarttrigger.addAction("my_trigger", {type = "CLICK", x = 100, y = 200})

-- 按键动作
wingman.smarttrigger.addAction("my_trigger", {type = "KEY_PRESS", key = 49})

-- 等待动作
wingman.smarttrigger.addAction("my_trigger", {type = "WAIT", waitMs = 500})

-- 日志动作
wingman.smarttrigger.addAction("my_trigger", {type = "LOG", message = "触发器被激活！"})

-- Lua 脚本动作
wingman.smarttrigger.addAction("my_trigger", {type = "LUA_SCRIPT", script = "print('hi')"})
```

:::

---

## 启动触发器

### start(triggerName) / start(triggerName)

**说明**：启动触发器。

**函数签名**：

```python
start(triggerName: str) -> bool
```

```lua
start(triggerName: string) -> boolean
```

**参数**：
- `triggerName` - 触发器名称

**返回**：
- 是否启动成功

---

## 停止触发器

### stop(triggerName) / stop(triggerName)

**说明**：停止触发器。

**函数签名**：

```python
stop(triggerName: str) -> bool
```

```lua
stop(triggerName: string) -> boolean
```

**参数**：
- `triggerName` - 触发器名称

**返回**：
- 是否停止成功

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

# 启动
ok = smarttrigger.start("my_trigger")

# 停止
stopped = smarttrigger.stop("my_trigger")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 启动
local ok = wingman.smarttrigger.start("my_trigger")

-- 停止
local stopped = wingman.smarttrigger.stop("my_trigger")
```

:::

---

## 移除触发器

### remove(triggerName) / remove(triggerName)

**说明**：移除触发器。

**函数签名**：

```python
remove(triggerName: str) -> None
```

```lua
remove(triggerName: string) -> nil
```

**参数**：
- `triggerName` - 触发器名称

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

smarttrigger.remove("my_trigger")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.smarttrigger.remove("my_trigger")
```

:::

---

## 查询运行状态

### isRunning(triggerName) / isRunning(triggerName)

**说明**：查询指定触发器是否正在运行。

**函数签名**：

```python
isRunning(triggerName: str) -> bool
```

```lua
isRunning(triggerName: string) -> boolean
```

**参数**：
- `triggerName` - 触发器名称

**返回**：
- 触发器存在且正在运行时返回 `true`，否则返回 `false`

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

if smarttrigger.isRunning("my_trigger"):
    print("触发器运行中")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.smarttrigger.isRunning("my_trigger") then
    print("触发器运行中")
end
```

:::

---

## 设置检查间隔

### setCheckInterval(triggerName, intervalMs) / setCheckInterval(triggerName, intervalMs)

**说明**：设置检查间隔（毫秒）。

**函数签名**：

```python
setCheckInterval(triggerName: str, intervalMs: int) -> bool
```

```lua
setCheckInterval(triggerName: string, intervalMs: number) -> boolean
```

**参数**：
- `triggerName` - 触发器名称
- `intervalMs` - 检查间隔（毫秒）

**返回**：
- 是否设置成功

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

ok = smarttrigger.setCheckInterval("my_trigger", 50)
if not ok:
    print("设置失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.smarttrigger.setCheckInterval("my_trigger", 50)
if not ok then
    print("设置失败")
end
```

:::

---

## 获取触发次数

### getTriggerCount(triggerName) / getTriggerCount(triggerName)

**说明**：获取已触发次数。

**函数签名**：

```python
getTriggerCount(triggerName: str) -> int
```

```lua
getTriggerCount(triggerName: string) -> number
```

**参数**：
- `triggerName` - 触发器名称

**返回**：
- 已触发次数

:::tabs

== Python

```python:line-numbers
from wingman import smarttrigger

count = smarttrigger.getTriggerCount("my_trigger")
print(f"已触发 {count} 次")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local count = wingman.smarttrigger.getTriggerCount("my_trigger")
print("已触发", count, "次")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `create(name)` | `create(name)` | 创建触发器 | name: 触发器名称<br>返回: 是否成功 |
| `addCondition(name, condition)` | `addCondition(name, condition)` | 添加条件 | name: 触发器名称<br>condition: 条件对象表<br>返回: 是否成功 |
| `addAction(name, action)` | `addAction(name, action)` | 添加动作 | name: 触发器名称<br>action: 动作对象表<br>返回: 是否成功 |
| `start(name)` | `start(name)` | 启动触发器 | name: 触发器名称<br>返回: 是否成功 |
| `stop(name)` | `stop(name)` | 停止触发器 | name: 触发器名称<br>返回: 是否成功 |
| `remove(name)` | `remove(name)` | 移除触发器 | name: 触发器名称 |
| `isRunning(name)` | `isRunning(name)` | 查询运行状态 | name: 触发器名称<br>返回: 是否运行中 |
| `setCheckInterval(name, ms)` | `setCheckInterval(name, ms)` | 设置检查间隔 | name: 触发器名称<br>ms: 毫秒数<br>返回: 是否成功 |
| `getTriggerCount(name)` | `getTriggerCount(name)` | 获取触发次数 | name: 触发器名称<br>返回: 触发次数 |

---

## 条件类型参考

`addCondition` 的 `condition.type` 字段取值（兼容大写枚举名与 snake_case）：

| 条件类型 | 相关字段 | 说明 |
|---------|---------|------|
| `COLOR_FOUND` | color, tolerance, region | 颜色出现 |
| `COLOR_NOT_FOUND` | color, tolerance, region | 颜色消失 |
| `IMAGE_FOUND` | template, threshold, region | 图像出现 |
| `IMAGE_NOT_FOUND` | template, threshold, region | 图像消失 |
| `TEXT_FOUND` | text, region | 文字出现 |
| `TEXT_NOT_FOUND` | text, region | 文字消失 |
| `OCR_CONTAINS` | text, region | OCR 包含文字 |
| `OCR_EQUALS` | text, region | OCR 等于文字 |
| `EDGE_DETECTED` | region | 检测到边缘 |
| `COLOR_CHANGED` | tolerance, region | 颜色变化 |

---

## 动作类型参考

`addAction` 的 `action.type` 字段取值（兼容大写枚举名与 snake_case）：

| 动作类型 | 相关字段 | 说明 |
|---------|---------|------|
| `CLICK` | x, y | 点击坐标 |
| `KEY_PRESS` | key | 按键 |
| `WAIT` | waitMs | 等待毫秒 |
| `LUA_SCRIPT` | script | 执行 Lua 脚本 |
| `LOG` | message | 输出日志 |
| `STOP` | - | 停止触发器 |

> 注意：当前脚本动作仅支持 `LUA_SCRIPT`，不支持 Python 脚本动作。
