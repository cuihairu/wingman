# 触发器系统指南

本指南详细介绍如何使用 Wingman 的触发器系统实现自动化操作。

> **当前实现（`wingman.smarttrigger`）**：触发器通过命令式 API 配置（条件 + 动作）：
>
> ```lua
> local wingman = require("wingman")
> wingman.smarttrigger.create("hp_low")
> wingman.smarttrigger.addCondition("hp_low", {
>     type = "color_found", color = 0xFF0000,
>     region = {x = 100, y = 100, width = 50, height = 200}, tolerance = 10
> })
> wingman.smarttrigger.addAction("hp_low", {type = "log", message = "Low health!"})
> wingman.smarttrigger.addAction("hp_low", {type = "key_press", keyCode = 0x31})  -- "1" 键
> wingman.smarttrigger.setCheckInterval("hp_low", 100)
> wingman.smarttrigger.start("hp_low")
> -- 其它: stop(name) / remove(name) / isRunning(name) / getTriggerCount(name)
> ```
>
> **可用函数**：`create(name)` / `addCondition(name, cond)` / `addAction(name, act)` / `setCheckInterval(name, ms)` / `start(name)` / `stop(name)` / `remove(name)` / `isRunning(name)` / `getTriggerCount(name)`。
>
> **条件 `type`**：`color_found` / `color_not_found` / `image_found` / `image_not_found` / `text_found` / `text_not_found` / `edge_detected` / `color_changed` / `ocr_contains` / `ocr_equals`（字段：`color` / `tolerance` / `threshold` / `region` / `text` / `template`）。
>
> **动作 `type`**：`click` / `key_press` / `wait` / `lua_script` / `log` / `stop`（字段：`x` / `y` / `key` / `keyCode` / `waitMs` / `script` / `message`；`key`/`keyCode` 为 vkCode 整数，如 `0x31` = "1"，传字符串会按 0 处理）。注意 `lua_script` 动作当前仅解析、未实现执行。
>
> **转换指南**：旧文档/旧脚本中的声明式单表配置写法 `create({condition = {...}, action = function() end})`（脚本层从无裸 `trigger` 模块，属历史遗留）应等价改写为 `smarttrigger.create(name)` + `addCondition(name, cond)` + `addAction(name, act)` 序列即可运行；其中 `action = function() ... end` 的回调按内部操作拆解为对应的 `key_press` / `click` / `log` 等动作。**smarttrigger 没有 time / hotkey 条件类型**：时间触发器请改用 `wingman.timer.every(ms, fn)`，按键触发器请改用 hotkey 模块的 `register(combo, callback)`。另注：声明式 TriggerEngine 的 `triggers` 配置表是配置文件方式、经 RPC/IPC 生效，与脚本层无关。

## 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [触发器类型](#触发器类型)
- [高级配置](#高级配置)
- [实战案例](#实战案例)
- [最佳实践](#最佳实践)

---

## 概述

触发器系统允许你基于特定条件自动执行操作，无需手动干预。

### 核心概念

- **触发条件**: 检测特定事件（颜色、图像、文字、OCR 等）
- **响应动作**: 条件满足时执行的操作（点击、按键、等待、日志等）
- **自动循环**: 持续监控并在条件满足时自动触发

### 应用场景

| 场景 | 描述 |
|------|------|
| **游戏辅助** | 检测敌人出现、血量低时自动使用药品 |
| **自动化测试** | UI 状态变化时执行测试操作 |
| **监控告警** | 检测异常状态并通知 |
| **批量操作** | 循环执行重复性任务 |

---

## 快速开始

### 简单颜色触发器

#### Lua

```lua
local wingman = require("wingman")

-- 创建颜色触发器
wingman.smarttrigger.create("hp_low")

-- 条件：血条区域出现红色
wingman.smarttrigger.addCondition("hp_low", {
    type = "color_found",
    color = 0xFF0000,        -- 红色
    region = {x = 100, y = 100, width = 50, height = 200},  -- 血条区域
    tolerance = 10           -- 颜色容差
})

-- 动作：日志 + 使用药品快捷键（keyCode 为 vkCode 整数，0x31 = "1"）
wingman.smarttrigger.addAction("hp_low", {type = "log", message = "Low health detected!"})
wingman.smarttrigger.addAction("hp_low", {type = "key_press", keyCode = 0x31})

-- 启动触发器
wingman.smarttrigger.start("hp_low")

print("Health monitor running...")
```

#### Python

```python
from wingman import smarttrigger

# 创建颜色触发器
smarttrigger.create("hp_low")

# 条件：血条区域出现红色
smarttrigger.addCondition("hp_low", {
    "type": "color_found",
    "color": 0xFF0000,
    "region": {"x": 100, "y": 100, "width": 50, "height": 200},
    "tolerance": 10
})

# 动作：日志 + 使用药品快捷键（keyCode 为 vkCode 整数，0x31 = "1"）
smarttrigger.addAction("hp_low", {"type": "log", "message": "Low health detected!"})
smarttrigger.addAction("hp_low", {"type": "key_press", "keyCode": 0x31})

# 启动触发器
smarttrigger.start("hp_low")

print("Health monitor running...")
```

### 简单图像触发器

#### Lua

```lua
local wingman = require("wingman")

-- 创建图像触发器
wingman.smarttrigger.create("enemy_found")

-- 条件：屏幕上出现敌人图标
wingman.smarttrigger.addCondition("enemy_found", {
    type = "image_found",
    template = "enemy.png",               -- 敌人图标
    region = {x = 0, y = 0, width = 1920, height = 1080},
    threshold = 0.85                      -- 匹配阈值
})

-- 动作：日志（点击类动作用固定坐标，见下文说明）
wingman.smarttrigger.addAction("enemy_found", {type = "log", message = "Enemy found"})

-- 启动触发器
wingman.smarttrigger.start("enemy_found")
```

#### Python

```python
from wingman import smarttrigger

# 创建图像触发器
smarttrigger.create("enemy_found")

# 条件：屏幕上出现敌人图标
smarttrigger.addCondition("enemy_found", {
    "type": "image_found",
    "template": "enemy.png",
    "region": {"x": 0, "y": 0, "width": 1920, "height": 1080},
    "threshold": 0.85
})

# 动作：日志（点击类动作用固定坐标，见下文说明）
smarttrigger.addAction("enemy_found", {"type": "log", "message": "Enemy found"})

# 启动触发器
smarttrigger.start("enemy_found")
```

---

## 触发器类型

### 颜色触发器

检测指定颜色出现/消失时触发。

#### 配置参数

条件以对象表形式传给 `addCondition(name, cond)`：

| 参数 | 类型 | 说明 |
|------|------|------|
| `type` | string | `"color_found"`（颜色出现）/ `"color_not_found"`（颜色消失） |
| `color` | number | 目标颜色值（0xRRGGBB） |
| `region` | table | 检测区域 `{x, y, width, height}` |
| `tolerance` | number | 颜色容差（0-255） |

检查频率用 `setCheckInterval(name, ms)` 设置（默认 100ms）。

#### 示例

#### Lua

```lua
local wingman = require("wingman")

-- 检测多个颜色点：一个触发器可挂多个条件（任一满足即触发）
local points = {
    {x = 100, y = 100, color = 0x00FF00},  -- 绿色
    {x = 200, y = 100, color = 0xFF0000},  -- 红色
    {x = 300, y = 100, color = 0x0000FF}   -- 蓝色
}

wingman.smarttrigger.create("color_points")
for _, point in ipairs(points) do
    wingman.smarttrigger.addCondition("color_points", {
        type = "color_found",
        color = point.color,
        region = {x = point.x - 5, y = point.y - 5, width = 10, height = 10},
        tolerance = 20
    })
end
wingman.smarttrigger.addAction("color_points", {type = "log", message = "Color match"})
wingman.smarttrigger.start("color_points")

-- 颜色范围检测：每 100ms 检查一次
wingman.smarttrigger.create("red_in_range")
wingman.smarttrigger.addCondition("red_in_range", {
    type = "color_found",
    color = 0xFF0000,
    region = {x = 0, y = 0, width = 1920, height = 1080},
    tolerance = 50   -- 较大的容差范围
})
wingman.smarttrigger.setCheckInterval("red_in_range", 100)
wingman.smarttrigger.addAction("red_in_range", {type = "log", message = "Red color detected in range"})
wingman.smarttrigger.start("red_in_range")
```

#### Python

```python
from wingman import smarttrigger

# 检测多个颜色点：一个触发器可挂多个条件（任一满足即触发）
points = [
    {"x": 100, "y": 100, "color": 0x00FF00},
    {"x": 200, "y": 100, "color": 0xFF0000},
    {"x": 300, "y": 100, "color": 0x0000FF}
]

smarttrigger.create("color_points")
for point in points:
    smarttrigger.addCondition("color_points", {
        "type": "color_found",
        "color": point["color"],
        "region": {"x": point["x"] - 5, "y": point["y"] - 5, "width": 10, "height": 10},
        "tolerance": 20
    })
smarttrigger.addAction("color_points", {"type": "log", "message": "Color match"})
smarttrigger.start("color_points")
```

### 图像触发器

检测指定图像出现/消失时触发。

#### 配置参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `type` | string | `"image_found"`（图像出现）/ `"image_not_found"`（图像消失） |
| `template` | string | 模板图像文件路径 |
| `region` | table | 搜索区域 `{x, y, width, height}` |
| `threshold` | number | 匹配阈值（0.0-1.0，默认 0.8） |

检查频率用 `setCheckInterval(name, ms)` 设置。

> **注意**：条件匹配到的坐标**不会传入动作**。`click` 动作只能使用固定坐标；需要在匹配位置点击的场景，请用 `wingman.screen.findImage` 轮询（见下文"自动收集掉落物"）。

#### 示例

#### Lua

```lua
local wingman = require("wingman")

-- 多图像检测：多个模板条件挂到同一触发器
local images = {"item1.png", "item2.png", "item3.png"}

wingman.smarttrigger.create("item_detector")
for _, image_path in ipairs(images) do
    wingman.smarttrigger.addCondition("item_detector", {
        type = "image_found",
        template = image_path,
        region = {x = 0, y = 0, width = 1920, height = 1080},
        threshold = 0.8
    })
end
wingman.smarttrigger.addAction("item_detector", {type = "log", message = "Item found"})
wingman.smarttrigger.start("item_detector")

-- 高精度图像检测：更高阈值 + 更频繁检查
wingman.smarttrigger.create("precise_detector")
wingman.smarttrigger.addCondition("precise_detector", {
    type = "image_found",
    template = "target.png",
    region = {x = 0, y = 0, width = 1920, height = 1080},
    threshold = 0.95   -- 高精度匹配
})
wingman.smarttrigger.setCheckInterval("precise_detector", 50)  -- 更频繁的检查
wingman.smarttrigger.addAction("precise_detector", {type = "log", message = "Precise match"})
wingman.smarttrigger.start("precise_detector")
```

#### Python

```python
from wingman import smarttrigger

# 多图像检测：多个模板条件挂到同一触发器
images = ["item1.png", "item2.png", "item3.png"]

smarttrigger.create("item_detector")
for image_path in images:
    smarttrigger.addCondition("item_detector", {
        "type": "image_found",
        "template": image_path,
        "region": {"x": 0, "y": 0, "width": 1920, "height": 1080},
        "threshold": 0.8
    })
smarttrigger.addAction("item_detector", {"type": "log", "message": "Item found"})
smarttrigger.start("item_detector")
```

### 时间触发器

按时间间隔定期执行操作。

> **注意**：smarttrigger **没有时间条件类型**。周期任务请使用定时器模块 `wingman.timer.every(ms, callback)`（一次性延时用 `wingman.timer.after(ms, callback)`）。

#### 示例

#### Lua

```lua
local wingman = require("wingman")

-- 定期保存状态（每 60 秒）
wingman.timer.every(60000, function()
    print("Auto-saving game state...")
    saveGameState()
end)

-- 定期检查状态（每 5 秒）
wingman.timer.every(5000, function()
    local health = getPlayerHealth()
    local mana = getPlayerMana()

    print(string.format("Status: HP=%d, MP=%d", health, mana))

    if health < 20 then
        print("Warning: Low health!")
    end
end)
```

#### Python

```python
from wingman import timer

# 定期保存状态（每 60 秒）
def save_job():
    print("Auto-saving game state...")
    save_game_state()

timer.every(60000, save_job)

# 定期检查状态（每 5 秒）
def monitor_job():
    print(f"Status: HP={get_player_health()}, MP={get_player_mana()}")

timer.every(5000, monitor_job)
```

### 按键触发器

检测指定按键按下时触发。

> **注意**：smarttrigger **没有按键条件类型**。系统级热键请使用 hotkey 模块的 `register(combo, callback)` / `unregister(id)`。

#### 示例

#### Python

```python
from wingman import hotkey

# F1 键触发帮助
def on_help():
    print("Help requested")
    show_help()

help_id = hotkey.register("F1", on_help)

# 快捷键组合
def on_start():
    print("Start")

def on_stop():
    print("Stop")

ids = [
    hotkey.register("F5", on_start),
    hotkey.register("F6", on_stop),
]
```

> **注意**：热键回调从后台线程触发，Lua 函数非线程安全会被拒绝（触发 `hotkey.error` 事件、返回 0）。注册热键请使用 Python。

#### Lua

```lua
-- Lua callable 非线程安全，wingman.hotkey.register 会拒绝 Lua 回调
-- （触发 hotkey.error 事件并返回 0），热键注册须用 Python。
-- Lua 中需要"按键触发"效果时，可在热键回调里经事件/队列桥接，
-- 或改用输入轮询：wingman.input.keyDown/keyUp 配合状态变量。
```

---

## 高级配置

### 触发器管理

#### 启用/停用触发器

#### Lua

```lua
local wingman = require("wingman")

-- 查询运行状态
if wingman.smarttrigger.isRunning("my_trigger") then
    print("Trigger is running")
end

-- 停用触发器
wingman.smarttrigger.stop("my_trigger")

-- 重新启用
wingman.smarttrigger.start("my_trigger")

-- 移除触发器
wingman.smarttrigger.remove("my_trigger")
```

#### Python

```python
from wingman import smarttrigger

# 查询运行状态
if smarttrigger.isRunning("my_trigger"):
    print("Trigger is running")

# 停用触发器
smarttrigger.stop("my_trigger")

# 重新启用
smarttrigger.start("my_trigger")

# 移除触发器
smarttrigger.remove("my_trigger")
```

### 条件过滤

smarttrigger 的多个条件之间是 **AND** 关系。需要"运行时状态"参与过滤（如血量阈值、战斗标记）时，已超出条件类型的能力，请用 `wingman.timer.every` 轮询自行判断。

#### Lua

```lua
local wingman = require("wingman")

-- 带运行时条件过滤的轮询：只在血量低于 50% 且在战斗中时使用药品
wingman.timer.every(1000, function()
    local current_hp = getPlayerHealth()
    local is_in_combat = isPlayerInCombat()

    if current_hp < 50 and is_in_combat then
        print("Using potion in combat")
        wingman.input.key(0x31)  -- "1" 键
    else
        print("Skipping potion use")
    end
end)
```

#### Python

```python
from wingman import timer, input

# 带运行时条件过滤的轮询
def combat_check():
    if get_player_health() < 50 and is_player_in_combat():
        print("Using potion in combat")
        input.key(0x31)  # "1" 键

timer.every(1000, combat_check)
```

### 防抖动

避免短时间内重复触发。`setCheckInterval` 已提供检查频率控制；更细的动作级防抖写在轮询回调里。注意脚本沙箱下 `os` 库不可用，取时间用 `wingman.system.getUptime()`（秒）。

#### Lua

```lua
local wingman = require("wingman")

local last_trigger_time = 0
local debounce_interval = 1  -- 1 秒防抖（getUptime 粒度为秒）

wingman.timer.every(100, function()
    -- 检测红色是否出现
    local res = wingman.screen.findColor(0xFF0000,
        {x = 0, y = 0, width = 1920, height = 1080}, 10)
    if not (res and res[2]) then
        return
    end

    -- 检查是否在防抖期内
    local current_time = wingman.system.getUptime()
    if current_time - last_trigger_time < debounce_interval then
        return  -- 跳过本次触发
    end

    -- 更新最后触发时间
    last_trigger_time = current_time

    print("Triggered (debounced)")
end)
```

#### Python

```python
from wingman import timer, screen, system

last_trigger_time = 0
debounce_interval = 1  # 1 秒防抖

def debounced_check():
    global last_trigger_time

    res = screen.findColor(0xFF0000,
                           {"x": 0, "y": 0, "width": 1920, "height": 1080}, 10)
    if not (res and res[1]):
        return

    current_time = system.getUptime()
    if current_time - last_trigger_time < debounce_interval:
        return  # 跳过本次触发

    last_trigger_time = current_time
    print("Triggered (debounced)")

timer.every(100, debounced_check)
```

### 触发计数

限制触发次数。用 `getTriggerCount(name)` 查询累计触发次数，达到上限后 `stop(name)`。

#### Lua

```lua
local wingman = require("wingman")

local max_triggers = 10

wingman.smarttrigger.create("limited")
wingman.smarttrigger.addCondition("limited", {
    type = "image_found",
    template = "target.png",
    region = {x = 0, y = 0, width = 1920, height = 1080},
    threshold = 0.8
})
wingman.smarttrigger.addAction("limited", {type = "log", message = "triggered"})
wingman.smarttrigger.start("limited")

-- 轮询触发次数，达到上限后停止
wingman.timer.every(1000, function()
    local count = wingman.smarttrigger.getTriggerCount("limited")
    print(string.format("Trigger %d/%d", count, max_triggers))
    if count >= max_triggers then
        print("Max triggers reached, stopping")
        wingman.smarttrigger.stop("limited")
    end
end)
```

#### Python

```python
from wingman import smarttrigger, timer

max_triggers = 10

smarttrigger.create("limited")
smarttrigger.addCondition("limited", {
    "type": "image_found",
    "template": "target.png",
    "region": {"x": 0, "y": 0, "width": 1920, "height": 1080},
    "threshold": 0.8
})
smarttrigger.addAction("limited", {"type": "log", "message": "triggered"})
smarttrigger.start("limited")

def check_count():
    count = smarttrigger.getTriggerCount("limited")
    print(f"Trigger {count}/{max_triggers}")
    if count >= max_triggers:
        print("Max triggers reached, stopping")
        smarttrigger.stop("limited")

timer.every(1000, check_count)
```

---

## 实战案例

### 游戏自动打怪

#### Lua

```lua
local wingman = require("wingman")

-- 查找怪物：需要在匹配位置点击，用轮询 + findImage 实现
-- （smarttrigger 的 click 动作只能用固定坐标，匹配坐标不传入动作）
wingman.timer.every(200, function()
    local res = wingman.screen.findImage("monster.png",
        {x = 0, y = 0, width = 1920, height = 1080}, 0.85)
    if res and res[2] then
        local match = res[1]
        print(string.format("Monster found at: %d, %d", match.x, match.y))

        -- 点击攻击
        wingman.input.click(match.x, match.y)

        -- 等待攻击完成后截图确认（captureRegion 返回是否截取成功）
        wingman.timer.after(2000, function()
            local ok = wingman.screen.captureRegion(
                {x = 0, y = 0, width = 1920, height = 1080})
            if ok then
                -- 继续查找...
            end
        end)
    end
end)

-- 监控血量：固定区域颜色出现即按键，适合 smarttrigger
wingman.smarttrigger.create("health_monitor")
wingman.smarttrigger.addCondition("health_monitor", {
    type = "color_found",
    color = 0xFF0000,  -- 红色表示低血量
    region = {x = 150, y = 50, width = 100, height = 20},  -- 血条位置
    tolerance = 15
})
wingman.smarttrigger.addAction("health_monitor", {type = "key_press", keyCode = 0x31})  -- "1" 药品快捷键
wingman.smarttrigger.setCheckInterval("health_monitor", 500)
wingman.smarttrigger.start("health_monitor")

-- 监控蓝量
wingman.smarttrigger.create("mana_monitor")
wingman.smarttrigger.addCondition("mana_monitor", {
    type = "color_found",
    color = 0x0000FF,  -- 蓝色表示低蓝量
    region = {x = 150, y = 80, width = 100, height = 20},  -- 蓝条位置
    tolerance = 15
})
wingman.smarttrigger.addAction("mana_monitor", {type = "key_press", keyCode = 0x32})  -- "2" 蓝药快捷键
wingman.smarttrigger.setCheckInterval("mana_monitor", 500)
wingman.smarttrigger.start("mana_monitor")

print("Auto-grind started!")
```

### 自动收集掉落物

#### Lua

```lua
local wingman = require("wingman")

-- 要收集的物品
local items = {"gold.png", "gem.png", "potion.png", "equipment.png"}

local collection_cooldown = 2  -- 2 秒冷却（getUptime 秒粒度）
local last_collected = {}

wingman.timer.every(300, function()
    local now = wingman.system.getUptime()

    for _, item_image in ipairs(items) do
        local res = wingman.screen.findImage(item_image,
            {x = 0, y = 0, width = 1920, height = 1080}, 0.8)
        if res and res[2] then
            local match = res[1]
            local item_key = string.format("%s_%d_%d",
                item_image, match.x, match.y)

            -- 检查冷却时间
            if last_collected[item_key] and
               now - last_collected[item_key] < collection_cooldown then
                -- 冷却中，跳过
            else
                -- 更新最后收集时间
                last_collected[item_key] = now

                print(string.format("Collecting %s at: %d, %d",
                    item_image, match.x, match.y))

                -- 移动到物品位置，稍后点击收集
                wingman.input.move(match.x, match.y, 300)
                wingman.timer.after(300, function()
                    wingman.input.click(match.x, match.y)
                end)
            end
        end
    end
end)
```

### 状态监控系统

#### Lua

```lua
local wingman = require("wingman")

-- 定期检查系统状态（smarttrigger 无时间条件类型，用 timer.every）
wingman.timer.every(10000, function()
    -- 检查 CPU 使用率
    local cpu_usage = wingman.system.getCpuUsage()
    print(string.format("CPU: %d%%", cpu_usage))

    if cpu_usage > 80 then
        wingman.notify.warn(string.format("高 CPU 使用率: %d%%", cpu_usage))
    end

    -- 检查内存使用
    local mem = wingman.system.getMemoryInfo()
    print(string.format("Memory: %d%%", mem.usage))

    if mem.usage > 80 then
        wingman.notify.warn(string.format("高内存使用: %d%%", mem.usage))
    end

    -- 检查目标进程
    local proc = wingman.process.find("target_process.exe")
    if not (proc and proc[2]) then
        wingman.notify.toast("进程异常", "目标进程已关闭", "error")
    end
end)
```

### 智能挂机系统

#### Lua

```lua
local wingman = require("wingman")

local state = {
    fighting = false,
    low_health = false,
    inventory_full = false
}

-- 主循环：每秒检查一次状态机
wingman.timer.every(1000, function()
    -- 更新状态
    state.fighting = isPlayerInCombat()
    state.low_health = getPlayerHealth() < 30
    state.inventory_full = isInventoryFull()

    -- 状态机处理
    if not state.fighting and not state.low_health then
        -- 寻找怪物
        print("Searching for monsters...")
        local monster = findNearestMonster()
        if monster then
            wingman.input.click(monster.x, monster.y)
        end
    elseif state.fighting then
        -- 战斗中
        print("Fighting...")
        executeCombatRotation()
    elseif state.low_health then
        -- 血量低，休息
        print("Low health, resting...")
        executeRestRoutine()
    end

    -- 背包满时处理
    if state.inventory_full then
        print("Inventory full, selling items...")
        executeSellRoutine()
    end
end)

-- 紧急情况处理：极低血量时连按逃生技能（动作按添加顺序执行）
wingman.smarttrigger.create("emergency")
wingman.smarttrigger.addCondition("emergency", {
    type = "color_found",
    color = 0xFF0000,  -- 极低血量
    region = {x = 150, y = 50, width = 100, height = 20},
    tolerance = 10
})
wingman.smarttrigger.addAction("emergency", {type = "log", message = "Emergency! Using survival items..."})
wingman.smarttrigger.addAction("emergency", {type = "key_press", keyCode = 0x7B})   -- F12 逃生技能
wingman.smarttrigger.addAction("emergency", {type = "wait", waitMs = 500})
wingman.smarttrigger.addAction("emergency", {type = "key_press", keyCode = 0x70})   -- F1 回城卷轴
wingman.smarttrigger.setCheckInterval("emergency", 200)
wingman.smarttrigger.start("emergency")
```

---

## 最佳实践

### 1. 触发器命名和管理

```lua
-- 为触发器分配有意义的名称，按名称统一管理
local trigger_names = {"monster_detector", "health_monitor", "mana_monitor"}

-- 方便管理
function stopAllTriggers()
    for _, name in ipairs(trigger_names) do
        wingman.smarttrigger.stop(name)
        print(string.format("Stopped: %s", name))
    end
end
```

### 2. 错误处理

smarttrigger 的动作是声明式对象表，没有用户回调可抛错，动作执行失败由引擎记录日志。轮询脚本里的复杂操作建议用 pcall 包裹：

```lua
wingman.timer.every(1000, function()
    local ok, err = pcall(function()
        -- 可能出错的操作
        performComplexAction()
    end)

    if not ok then
        print("Error in action:", err)
        -- 可以选择停止触发器或重试
    end
end)
```

### 3. 性能优化

```lua
-- 用 setCheckInterval 控制检查频率（默认 100ms，避免设置过高频率）
wingman.smarttrigger.create("optimized")
wingman.smarttrigger.addCondition("optimized", {
    type = "color_found",
    color = 0xFF0000,
    region = {x = 0, y = 0, width = 1920, height = 1080},
    tolerance = 10
})
wingman.smarttrigger.setCheckInterval("optimized", 500)  -- 合理的检查间隔
wingman.smarttrigger.start("optimized")

-- 减小搜索区域：只搜索特定区域而不是全屏
wingman.smarttrigger.create("focused")
wingman.smarttrigger.addCondition("focused", {
    type = "image_found",
    template = "target.png",
    region = {x = 800, y = 200, width = 320, height = 400},
    threshold = 0.8
})
wingman.smarttrigger.start("focused")
```

### 4. 调试支持

```lua
local wingman = require("wingman")

wingman.smarttrigger.create("debug_trigger")
wingman.smarttrigger.addCondition("debug_trigger", {
    type = "image_found",
    template = "target.png",
    region = {x = 0, y = 0, width = 1920, height = 1080},
    threshold = 0.8
})

-- 输出调试信息（debugger 模块无 log 函数，用 LOG 动作或 print）
wingman.smarttrigger.addAction("debug_trigger", {type = "log", message = "Match found"})
wingman.smarttrigger.start("debug_trigger")

-- 调试模式下设置断点标记：
-- wingman.debugger.breakHere()                    -- 无参调用
-- wingman.debugger.breakpoint("my_script.lua", 42)  -- (file, line) 两参
```

---

## 相关文档

- [脚本 API - SmartTrigger](../api/smart-trigger.md)
- [任务 API - Task](../api/task.md)
- [核心 API - Screen](../api/core.md)

---

**返回**: [文档首页](../README.md) | [使用指南](../README.md#使用指南)
