# API: wingman.smart_trigger

智能触发器模块，提供事件驱动的自动化能力。

## 模块概述

smart_trigger 模块提供智能触发器功能：
- **创建触发器** - 创建命名触发器
- **添加条件** - 颜色/图像/OCR 等检测条件
- **添加动作** - 点击/按键/等待/日志等动作
- **启动停止** - 启动或停止触发器
- **参数配置** - 检查间隔、最大触发次数

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
from wingman import smart_trigger

ok = smart_trigger.create("my_trigger")
if not ok:
    print("创建失败")
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

local ok = smarttrigger.create("my_trigger")
if not ok then
    print("创建失败")
end
```

:::

---

## 添加触发条件

### add_condition(trigger_name, condition_type, ...) / addCondition(triggerName, conditionType, ...)

**说明**：添加触发条件。

**函数签名**：

```python
add_condition(trigger_name: str, condition_type: str, *args) -> None
```

```lua
addCondition(triggerName: string, conditionType: string, ...) -> nil
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称
- `condition_type` / `conditionType` - 条件类型
- `...` - 条件参数（根据类型不同）

**返回**：
- 无

**支持的条件类型：**

| 条件类型 | 参数 | 说明 |
|---------|------|------|
| `COLOR_FOUND` | color, tolerance, region | 颜色出现 |
| `COLOR_NOT_FOUND` | color, tolerance, region | 颜色消失 |
| `IMAGE_FOUND` | templatePath, threshold, region | 图像出现 |
| `IMAGE_NOT_FOUND` | templatePath, threshold, region | 图像消失 |
| `TEXT_FOUND` | targetText, region | 文字出现 |
| `TEXT_NOT_FOUND` | targetText, region | 文字消失 |
| `OCR_CONTAINS` | targetText, region | OCR 包含文字 |
| `OCR_EQUALS` | targetText, region | OCR 等于文字 |
| `EDGE_DETECTED` | region | 检测到边缘 |
| `COLOR_CHANGED` | tolerance, region | 颜色变化 |

:::tabs

== Python

```python:line-numbers
from wingman import smart_trigger

# 颜色检测条件
smart_trigger.add_condition("my_trigger", "COLOR_FOUND",
    {"r": 255, "g": 0, "b": 0}, 10, {"x": 100, "y": 100, "width": 50, "height": 50})

# 图像检测条件
smart_trigger.add_condition("my_trigger", "IMAGE_FOUND",
    "target.png", 0.8, {"x": 0, "y": 0, "width": 800, "height": 600})

# OCR 条件
smart_trigger.add_condition("my_trigger", "OCR_CONTAINS",
    "敌人", {"x": 0, "y": 0, "width": 200, "height": 50})
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

-- 颜色检测条件
smarttrigger.addCondition("my_trigger", "COLOR_FOUND",
    {r=255, g=0, b=0}, 10, {x=100, y=100, width=50, height=50})

-- 图像检测条件
smarttrigger.addCondition("my_trigger", "IMAGE_FOUND",
    "target.png", 0.8, {x=0, y=0, width=800, height=600})

-- OCR 条件
smarttrigger.addCondition("my_trigger", "OCR_CONTAINS",
    "敌人", {x=0, y=0, width=200, height=50})
```

:::

---

## 添加触发动作

### add_action(trigger_name, action_type, ...) / addAction(triggerName, actionType, ...)

**说明**：添加触发动作。

**函数签名**：

```python
add_action(trigger_name: str, action_type: str, *args) -> None
```

```lua
addAction(triggerName: string, actionType: string, ...) -> nil
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称
- `action_type` / `actionType` - 动作类型
- `...` - 动作参数（根据类型不同）

**返回**：
- 无

**支持的动作类型：**

| 动作类型 | 参数 | 说明 |
|---------|------|------|
| `CLICK` | x, y | 点击坐标 |
| `KEY_PRESS` | keyCode | 按键 |
| `WAIT` | milliseconds | 等待毫秒 |
| `LOG` | message | 输出日志 |
| `STOP` | - | 停止触发器 |
| `LUA_SCRIPT` / `PYTHON_SCRIPT` | script | 执行脚本 |

:::tabs

== Python

```python:line-numbers
from wingman import smart_trigger

# 点击动作
smart_trigger.add_action("my_trigger", "CLICK", 100, 200)

# 按键动作
smart_trigger.add_action("my_trigger", "KEY_PRESS", 49)

# 等待动作
smart_trigger.add_action("my_trigger", "WAIT", 500)

# 日志动作
smart_trigger.add_action("my_trigger", "LOG", "触发器被激活！")
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

-- 点击动作
smarttrigger.addAction("my_trigger", "CLICK", 100, 200)

-- 按键动作
smarttrigger.addAction("my_trigger", "KEY_PRESS", 49)

-- 等待动作
smarttrigger.addAction("my_trigger", "WAIT", 500)

-- 日志动作
smarttrigger.addAction("my_trigger", "LOG", "触发器被激活！")
```

:::

---

## 启动触发器

### start(trigger_name) / start(triggerName)

**说明**：启动触发器。

**函数签名**：

```python
start(trigger_name: str) -> bool
```

```lua
start(triggerName: string) -> boolean
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称

**返回**：
- 是否启动成功

---

## 停止触发器

### stop(trigger_name) / stop(triggerName)

**说明**：停止触发器。

**函数签名**：

```python
stop(trigger_name: str) -> None
```

```lua
stop(triggerName: string) -> nil
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import smart_trigger

# 启动
ok = smart_trigger.start("my_trigger")

# 停止
smart_trigger.stop("my_trigger")
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

-- 启动
local ok = smarttrigger.start("my_trigger")

-- 停止
smarttrigger.stop("my_trigger")
```

:::

---

## 移除触发器

### remove(trigger_name) / remove(triggerName)

**说明**：移除触发器。

**函数签名**：

```python
remove(trigger_name: str) -> None
```

```lua
remove(triggerName: string) -> nil
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import smart_trigger

smart_trigger.remove("my_trigger")
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

smarttrigger.remove("my_trigger")
```

:::

---

## 设置检查间隔

### set_check_interval(trigger_name, interval_ms) / setCheckInterval(triggerName, intervalMs)

**说明**：设置检查间隔（毫秒）。

**函数签名**：

```python
set_check_interval(trigger_name: str, interval_ms: int) -> None
```

```lua
setCheckInterval(triggerName: string, intervalMs: number) -> nil
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称
- `interval_ms` / `intervalMs` - 检查间隔（毫秒）

**返回**：
- 无

---

## 设置最大触发次数

### set_max_triggers(trigger_name, max_count) / setMaxTriggers(triggerName, maxCount)

**说明**：设置最大触发次数（0 = 无限）。

**函数签名**：

```python
set_max_triggers(trigger_name: str, max_count: int) -> None
```

```lua
setMaxTriggers(triggerName: string, maxCount: number) -> nil
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称
- `max_count` / `maxCount` - 最大触发次数，0 表示无限

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import smart_trigger

# 设置检查间隔（毫秒）
smart_trigger.set_check_interval("my_trigger", 50)

# 设置最大触发次数（0 = 无限）
smart_trigger.set_max_triggers("my_trigger", 10)
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

-- 设置检查间隔（毫秒）
smarttrigger.setCheckInterval("my_trigger", 50)

-- 设置最大触发次数（0 = 无限）
smarttrigger.setMaxTriggers("my_trigger", 10)
```

:::

---

## 获取触发次数

### get_trigger_count(trigger_name) / getTriggerCount(triggerName)

**说明**：获取已触发次数。

**函数签名**：

```python
get_trigger_count(trigger_name: str) -> int
```

```lua
getTriggerCount(triggerName: string) -> number
```

**参数**：
- `trigger_name` / `triggerName` - 触发器名称

**返回**：
- 已触发次数

:::tabs

== Python

```python:line-numbers
from wingman import smart_trigger

count = smart_trigger.get_trigger_count("my_trigger")
print(f"已触发 {count} 次")
```

== Lua

```lua:line-numbers
local smarttrigger = require("wingman.smarttrigger")

local count = smarttrigger.getTriggerCount("my_trigger")
print("已触发", count, "次")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `create(name)` | `create(name)` | 创建触发器 | name: 触发器名称<br>返回: 是否成功 |
| `add_condition(name, type, ...)` | `addCondition(name, type, ...)` | 添加条件 | name: 触发器名称<br>type: 条件类型<br>...: 条件参数 |
| `add_action(name, type, ...)` | `addAction(name, type, ...)` | 添加动作 | name: 触发器名称<br>type: 动作类型<br>...: 动作参数 |
| `start(name)` | `start(name)` | 启动触发器 | name: 触发器名称<br>返回: 是否成功 |
| `stop(name)` | `stop(name)` | 停止触发器 | name: 触发器名称 |
| `remove(name)` | `remove(name)` | 移除触发器 | name: 触发器名称 |
| `set_check_interval(name, interval)` | `setCheckInterval(name, interval)` | 设置检查间隔 | name: 触发器名称<br>interval: 毫秒数 |
| `set_max_triggers(name, max)` | `setMaxTriggers(name, max)` | 设置最大触发次数 | name: 触发器名称<br>max: 最大次数(0=无限) |
| `get_trigger_count(name)` | `getTriggerCount(name)` | 获取触发次数 | name: 触发器名称<br>返回: 触发次数 |

---

## 条件类型参考

| 条件类型 | 参数 | 说明 |
|---------|------|------|
| `COLOR_FOUND` | color, tolerance, region | 颜色出现 |
| `COLOR_NOT_FOUND` | color, tolerance, region | 颜色消失 |
| `IMAGE_FOUND` | templatePath, threshold, region | 图像出现 |
| `IMAGE_NOT_FOUND` | templatePath, threshold, region | 图像消失 |
| `TEXT_FOUND` | targetText, region | 文字出现 |
| `TEXT_NOT_FOUND` | targetText, region | 文字消失 |
| `OCR_CONTAINS` | targetText, region | OCR 包含文字 |
| `OCR_EQUALS` | targetText, region | OCR 等于文字 |
| `EDGE_DETECTED` | region | 检测到边缘 |
| `COLOR_CHANGED` | tolerance, region | 颜色变化 |

---

## 动作类型参考

| 动作类型 | 参数 | 说明 |
|---------|------|------|
| `CLICK` | x, y | 点击坐标 |
| `KEY_PRESS` | keyCode | 按键 |
| `WAIT` | milliseconds | 等待毫秒 |
| `LOG` | message | 输出日志 |
| `STOP` | - | 停止触发器 |
| `LUA_SCRIPT` / `PYTHON_SCRIPT` | script | 执行脚本 |
