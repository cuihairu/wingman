# API: wingman.smart_trigger

智能触发器系统提供事件驱动的自动化能力。

## 创建触发器

<CodeTabs>

:::slot python

```python
from wingman import smart_trigger

ok = smart_trigger.create("my_trigger")
if not ok:
    print("创建失败")
```

:::

:::slot lua

```lua
local smarttrigger = require("wingman.smarttrigger")

local ok = smarttrigger.create("my_trigger")
if not ok then
    print("创建失败")
end
```

:::

</CodeTabs>

## 添加触发条件

<CodeTabs>

:::slot python

```python
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

:::

:::slot lua

```lua
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

</CodeTabs>

## 添加触发动作

<CodeTabs>

:::slot python

```python
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

:::

:::slot lua

```lua
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

</CodeTabs>

## 启动/停止触发器

<CodeTabs>

:::slot python

```python
from wingman import smart_trigger

# 启动
ok = smart_trigger.start("my_trigger")

# 停止
smart_trigger.stop("my_trigger")
```

:::

:::slot lua

```lua
local smarttrigger = require("wingman.smarttrigger")

-- 启动
local ok = smarttrigger.start("my_trigger")

-- 停止
smarttrigger.stop("my_trigger")
```

:::

</CodeTabs>

## 移除触发器

<CodeTabs>

:::slot python

```python
from wingman import smart_trigger

smart_trigger.remove("my_trigger")
```

:::

:::slot lua

```lua
local smarttrigger = require("wingman.smarttrigger")

smarttrigger.remove("my_trigger")
```

:::

</CodeTabs>

## 设置参数

<CodeTabs>

:::slot python

```python
from wingman import smart_trigger

# 设置检查间隔（毫秒）
smart_trigger.set_check_interval("my_trigger", 50)

# 设置最大触发次数（0 = 无限）
smart_trigger.set_max_triggers("my_trigger", 10)
```

:::

:::slot lua

```lua
local smarttrigger = require("wingman.smarttrigger")

-- 设置检查间隔（毫秒）
smarttrigger.setCheckInterval("my_trigger", 50)

-- 设置最大触发次数（0 = 无限）
smarttrigger.setMaxTriggers("my_trigger", 10)
```

:::

</CodeTabs>

## 获取触发次数

<CodeTabs>

:::slot python

```python
from wingman import smart_trigger

count = smart_trigger.get_trigger_count("my_trigger")
print(f"已触发 {count} 次")
```

:::

:::slot lua

```lua
local smarttrigger = require("wingman.smarttrigger")

local count = smarttrigger.getTriggerCount("my_trigger")
print("已触发", count, "次")
```

:::

</CodeTabs>

---

## 完整示例

<CodeTabs>

:::slot python

```python
from wingman import smart_trigger

# 自动喝药触发器
smart_trigger.create("auto_heal")
smart_trigger.add_condition("auto_heal", "COLOR_FOUND",
    {"r": 255, "g": 0, "b": 0}, 10, {"x": 100, "y": 100, "width": 50, "height": 10})
smart_trigger.add_action("auto_heal", "KEY_PRESS", 49)
smart_trigger.set_check_interval("auto_heal", 100)
smart_trigger.set_max_triggers("auto_heal", 0)  # 无限触发
smart_trigger.start("auto_heal")

# 敌人检测触发器
smart_trigger.create("enemy_alert")
smart_trigger.add_condition("enemy_alert", "IMAGE_FOUND",
    "enemy.png", 0.7, {"x": 0, "y": 0, "width": 800, "height": 600})
smart_trigger.add_action("enemy_alert", "LOG", "发现敌人！")
smart_trigger.start("enemy_alert")
```

:::

:::slot lua

```lua
local smarttrigger = require("wingman.smarttrigger")

-- 自动喝药触发器
smarttrigger.create("auto_heal")
smarttrigger.addCondition("auto_heal", "COLOR_FOUND",
    {r=255, g=0, b=0}, 10, {x=100, y=100, width=50, height=10})
smarttrigger.addAction("auto_heal", "KEY_PRESS", 49)
smarttrigger.setCheckInterval("auto_heal", 100)
smarttrigger.setMaxTriggers("auto_heal", 0)  -- 无限触发
smarttrigger.start("auto_heal")

-- 敌人检测触发器
smarttrigger.create("enemy_alert")
smarttrigger.addCondition("enemy_alert", "IMAGE_FOUND",
    "enemy.png", 0.7, {x=0, y=0, width=800, height=600})
smarttrigger.addAction("enemy_alert", "LOG", "发现敌人！")
smarttrigger.start("enemy_alert")
```

:::

</CodeTabs>

---

## 可用接口

### `create(name)`

创建一个新的触发器。

### `add_condition(trigger_name, condition_type, ...)` / `addCondition(...)`

添加触发条件。

**支持的条件类型:**

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

### `add_action(trigger_name, action_type, ...)` / `addAction(...)`

添加触发动作。

**支持的动作类型:**

| 动作类型 | 参数 | 说明 |
|---------|------|------|
| `CLICK` | x, y | 点击坐标 |
| `KEY_PRESS` | keyCode | 按键 |
| `WAIT` | milliseconds | 等待毫秒 |
| `LOG` | message | 输出日志 |
| `STOP` | - | 停止触发器 |
| `LUA_SCRIPT` / `PYTHON_SCRIPT` | script | 执行脚本 |

### `start(trigger_name)` / `stop(trigger_name)`

启动/停止触发器。

### `remove(trigger_name)`

移除触发器。

### `set_check_interval(trigger_name, interval_ms)` / `setCheckInterval(...)`

设置检查间隔（毫秒）。

### `set_max_triggers(trigger_name, max_count)` / `setMaxTriggers(...)`

设置最大触发次数（0 = 无限）。

### `get_trigger_count(trigger_name)` / `getTriggerCount(...)`

获取已触发次数。
