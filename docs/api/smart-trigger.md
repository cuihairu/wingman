# SmartTrigger API

智能触发器系统提供事件驱动的自动化能力。

## Lua API

### smarttrigger.create(name)

创建一个新的触发器。

```lua
local ok = smarttrigger.create("my_trigger")
if not ok then
    print("创建失败")
end
```

---

### smarttrigger.addCondition(triggerName, conditionType, ...)

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

```lua
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

---

### smarttrigger.addAction(triggerName, actionType, ...)

添加触发动作。

**支持的动作类型:**

| 动作类型 | 参数 | 说明 |
|---------|------|------|
| `CLICK` | x, y | 点击坐标 |
| `KEY_PRESS` | keyCode | 按键 |
| `WAIT` | milliseconds | 等待毫秒 |
| `LOG` | message | 输出日志 |
| `STOP` | - | 停止触发器 |
| `LUA_SCRIPT` | script | 执行 Lua 脚本 |

```lua
-- 点击动作
smarttrigger.addAction("my_trigger", "CLICK", 100, 200)

-- 按键动作
smarttrigger.addAction("my_trigger", "KEY_PRESS", 49)

-- 等待动作
smarttrigger.addAction("my_trigger", "WAIT", 500)

-- 日志动作
smarttrigger.addAction("my_trigger", "LOG", "触发器被激活！")
```

---

### smarttrigger.start(triggerName)

启动触发器。

```lua
local ok = smarttrigger.start("my_trigger")
```

---

### smarttrigger.stop(triggerName)

停止触发器。

```lua
smarttrigger.stop("my_trigger")
```

---

### smarttrigger.remove(triggerName)

移除触发器。

```lua
smarttrigger.remove("my_trigger")
```

---

### smarttrigger.setCheckInterval(triggerName, intervalMs)

设置检查间隔（毫秒）。

```lua
smarttrigger.setCheckInterval("my_trigger", 50)  -- 50ms 检查一次
```

---

### smarttrigger.setMaxTriggers(triggerName, maxCount)

设置最大触发次数（0 = 无限）。

```lua
smarttrigger.setMaxTriggers("my_trigger", 10)  -- 只触发 10 次
```

---

### smarttrigger.getTriggerCount(triggerName)

获取已触发次数。

```lua
local count = smarttrigger.getTriggerCount("my_trigger")
print("已触发", count, "次")
```

## 完整示例

```lua
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

-- 等待用户输入
io.read()
```

## C++ API

### 创建触发器

```cpp
#include "wingman/smart_trigger.hpp"

auto trigger = SmartTriggerManager::instance().createTrigger("my_trigger");

// 添加条件
TriggerCondition condition;
condition.type = TriggerConditionType::COLOR_FOUND;
condition.targetColor = Color(255, 0, 0);
condition.tolerance = 10;
trigger->addCondition(condition);

// 添加动作
TriggerAction action;
action.type = TriggerActionType::KEY_PRESS;
action.keyCode = 49;
trigger->addAction(action);

// 启动
trigger->start();
```
