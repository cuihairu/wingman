# 触发器系统指南

本指南详细介绍如何使用 Wingman 的触发器系统实现自动化操作。

## 📋 目录

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

- **触发条件**: 检测特定事件（颜色、图像、时间、按键等）
- **响应动作**: 条件满足时执行的操作
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
local trigger = require("wingman.trigger")
local input = require("wingman.input")

-- 创建颜色触发器
local health_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,        -- 红色
    x = 100, y = 100,         -- 血条区域
    width = 50, height = 200,
    tolerance = 10,           -- 颜色容差

    action = function(match)
        print("Low health detected!")
        -- 自动使用药品
        input.keyPress("1")
    end
})

-- 启动触发器
trigger.start(health_trigger)

print("Health monitor running...")
print("Press Ctrl+C to stop")
```

#### Python

```python
from wingman import trigger, input

# 创建颜色触发器
health_trigger = trigger.create({
    "type": "color",
    "color": 0xFF0000,
    "x": 100, "y": 100,
    "width": 50, "height": 200,
    "tolerance": 10,
    "action": lambda match: (
        print("Low health detected!"),
        input.keyPress("1")
    )
})

# 启动触发器
trigger.start(health_trigger)

print("Health monitor running...")
print("Press Ctrl+C to stop")
```

### 简单图像触发器

#### Lua

```lua
local trigger = require("wingman.trigger")
local input = require("wingman.input")

-- 创建图像触发器
local enemy_trigger = trigger.create({
    type = "image",
    image = "enemy.png",               -- 敌人图标
    x = 0, y = 0,
    width = 1920, height = 1080,
    threshold = 0.85,                  -- 匹配阈值

    action = function(match)
        print(string.format("Enemy found at: %d, %d", match.x, match.y))
        -- 自动攻击
        input.click(match.x, match.y, "left")
    end
})

trigger.start(enemy_trigger)
```

#### Python

```python
from wingman import trigger, input

enemy_trigger = trigger.create({
    "type": "image",
    "image": "enemy.png",
    "x": 0, "y": 0,
    "width": 1920, "height": 1080,
    "threshold": 0.85,
    "action": lambda match: (
        print(f"Enemy found at: {match['x']}, {match['y']}"),
        input.click(match["x"], match["y"], "left")
    )
})

trigger.start(enemy_trigger)
```

---

## 触发器类型

### 颜色触发器

检测指定颜色出现时触发。

#### 配置参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定为 "color" |
| `color` | number | 目标颜色值（0xRRGGBB） |
| `x, y` | number | 搜索区域起始位置 |
| `width, height` | number | 搜索区域大小 |
| `tolerance` | number | 颜色容差（0-255） |
| `interval` | number | 检查间隔（毫秒，可选） |
| `action` | function | 触发时执行的操作 |

#### 示例

#### Lua

```lua
-- 检测多个颜色点
local points = {
    {x = 100, y = 100, color = 0x00FF00},  -- 绿色
    {x = 200, y = 100, color = 0xFF0000},  -- 红色
    {x = 300, y = 100, color = 0x0000FF}   -- 蓝色
}

for _, point in ipairs(points) do
    local t = trigger.create({
        type = "color",
        color = point.color,
        x = point.x - 5, y = point.y - 5,
        width = 10, height = 10,
        tolerance = 20,
        action = function(match)
            print(string.format("Color match at: %d, %d", match.x, match.y))
        end
    })
    trigger.start(t)
end

-- 颜色范围检测
local color_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,
    x = 0, y = 0,
    width = 1920, height = 1080,
    tolerance = 50,  -- 较大的容差范围
    interval = 100,  -- 每 100ms 检查一次
    action = function(match)
        print("Red color detected in range")
    end
})
```

#### Python

```python
# 检测多个颜色点
points = [
    {"x": 100, "y": 100, "color": 0x00FF00},
    {"x": 200, "y": 100, "color": 0xFF0000},
    {"x": 300, "y": 100, "color": 0x0000FF}
]

for point in points:
    t = trigger.create({
        "type": "color",
        "color": point["color"],
        "x": point["x"] - 5, "y": point["y"] - 5,
        "width": 10, "height": 10,
        "tolerance": 20,
        "action": lambda match: print(f"Color match at: {match['x']}, {match['y']}")
    })
    trigger.start(t)
```

### 图像触发器

检测指定图像出现时触发。

#### 配置参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定为 "image" |
| `image` | string | 图像文件路径 |
| `x, y` | number | 搜索区域起始位置 |
| `width, height` | number | 搜索区域大小 |
| `threshold` | number | 匹配阈值（0.0-1.0） |
| `interval` | number | 检查间隔（毫秒，可选） |
| `action` | function | 触发时执行的操作 |

#### 示例

#### Lua

```lua
-- 多图像检测
local images = {"item1.png", "item2.png", "item3.png"}

for _, image_path in ipairs(images) do
    local t = trigger.create({
        type = "image",
        image = image_path,
        x = 0, y = 0,
        width = 1920, height = 1080,
        threshold = 0.8,
        action = function(match)
            print(string.format("Found %s at: %d, %d",
                image_path, match.x, match.y))
            -- 点击图像
            input.click(match.x, match.y, "left")
        end
    })
    trigger.start(t)
end

-- 高精度图像检测
local precise_trigger = trigger.create({
    type = "image",
    image = "target.png",
    x = 0, y = 0,
    width = 1920, height = 1080,
    threshold = 0.95,  -- 高精度匹配
    interval = 50,      -- 更频繁的检查
    action = function(match)
        print(string.format("Precise match: %.2f%% at %d, %d",
            match.confidence * 100, match.x, match.y))
    end
})
```

#### Python

```python
# 多图像检测
images = ["item1.png", "item2.png", "item3.png"]

for image_path in images:
    t = trigger.create({
        "type": "image",
        "image": image_path,
        "x": 0, "y": 0,
        "width": 1920, "height": 1080,
        "threshold": 0.8,
        "action": lambda match, path=image_path: (
            print(f"Found {path} at: {match['x']}, {match['y']}"),
            input.click(match["x"], match["y"], "left")
        )
    })
    trigger.start(t)
```

### 时间触发器

按时间间隔定期执行操作。

#### 配置参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定为 "time" |
| `interval` | number | 执行间隔（毫秒） |
| `action` | function | 定期执行的操作 |

#### 示例

#### Lua

```lua
-- 定期保存状态
local save_trigger = trigger.create({
    type = "time",
    interval = 60000,  -- 每 60 秒
    action = function()
        print("Auto-saving game state...")
        saveGameState()
    end
})

-- 定期检查状态
local monitor_trigger = trigger.create({
    type = "time",
    interval = 5000,  -- 每 5 秒
    action = function()
        local health = getPlayerHealth()
        local mana = getPlayerMana()

        print(string.format("Status: HP=%d, MP=%d", health, mana))

        if health < 20 then
            print("Warning: Low health!")
        end
    end
})
```

#### Python

```python
# 定期保存状态
save_trigger = trigger.create({
    "type": "time",
    "interval": 60000,  # 每 60 秒
    "action": lambda: (
        print("Auto-saving game state..."),
        save_game_state()
    )
})

# 定期检查状态
monitor_trigger = trigger.create({
    "type": "time",
    "interval": 5000,  # 每 5 秒
    "action": lambda: (
        print(f"Status: HP={get_player_health()}, MP={get_player_mana()}")
    )
})
```

### 按键触发器

检测指定按键按下时触发。

#### 配置参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定为 "key" |
| `key` | string | 按键名称 |
| `action` | function | 按键按下时执行的操作 |

#### 示例

#### Lua

```lua
-- F1 键触发帮助
local help_trigger = trigger.create({
    type = "key",
    key = "F1",
    action = function()
        print("Help requested")
        showHelp()
    end
})

-- ESC 键触发退出
local exit_trigger = trigger.create({
    type = "key",
    key = "ESC",
    action = function()
        print("Exiting...")
        stopAllTriggers()
        os.exit(0)
    end
})

-- 快捷键触发器
local hotkeys = {
    {key = "F5", action = function() print("Start") end},
    {key = "F6", action = function() print("Stop") end},
    {key = "F7", action = function() print("Pause") end}
}

for _, hotkey in ipairs(hotkeys) do
    local t = trigger.create({
        type = "key",
        key = hotkey.key,
        action = hotkey.action
    })
    trigger.start(t)
end
```

#### Python

```python
# F1 键触发帮助
help_trigger = trigger.create({
    "type": "key",
    "key": "F1",
    "action": lambda: (
        print("Help requested"),
        show_help()
    )
})

# 快捷键触发器
hotkeys = {
    {"key": "F5", "action": lambda: print("Start")},
    {"key": "F6", "action": lambda: print("Stop")},
    {"key": "F7", "action": lambda: print("Pause")}
}

for hotkey in hotkeys:
    t = trigger.create({
        "type": "key",
        "key": hotkey["key"],
        "action": hotkey["action"]
    })
    trigger.start(t)
```

---

## 高级配置

### 触发器管理

#### 启用/禁用触发器

#### Lua

```lua
local trigger = require("wingman.trigger")

local t = trigger.create({...})

-- 检查是否启用
if trigger.isEnabled(t) then
    print("Trigger is enabled")
end

-- 禁用触发器
trigger.disable(t)

-- 重新启用
trigger.enable(t)
```

#### Python

```python
from wingman import trigger

t = trigger.create({...})

# 检查是否启用
if trigger.is_enabled(t):
    print("Trigger is enabled")

# 禁用触发器
trigger.disable(t)

# 重新启用
trigger.enable(t)
```

### 条件过滤

在触发动作前添加额外条件判断。

#### Lua

```lua
local trigger = require("wingman.trigger")
local input = require("wingman.input")

-- 带条件过滤的触发器
local smart_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,
    x = 100, y = 100,
    width = 50, height = 200,
    tolerance = 10,

    action = function(match)
        -- 额外条件检查
        local current_hp = getPlayerHealth()
        local is_in_combat = isPlayerInCombat()

        -- 只在血量低于 50% 且在战斗中时使用药品
        if current_hp < 50 and is_in_combat then
            print("Using potion in combat")
            input.keyPress("1")
        else
            print("Skipping potion use")
        end
    end
})
```

#### Python

```python
from wingman import trigger, input

# 带条件过滤的触发器
smart_trigger = trigger.create({
    "type": "color",
    "color": 0xFF0000,
    "x": 100, "y": 100,
    "width": 50, "height": 200,
    "tolerance": 10,
    "action": lambda match: (
        print("Using potion in combat"),
        input.keyPress("1")
    ) if get_player_health() < 50 and is_player_in_combat() else None
})
```

### 防抖动

避免短时间内重复触发。

#### Lua

```lua
local trigger = require("wingman.trigger")

local last_trigger_time = 0
local debounce_interval = 1000  -- 1 秒防抖

local debounced_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,
    x = 0, y = 0,
    width = 1920, height = 1080,
    tolerance = 10,

    action = function(match)
        local current_time = os.time() * 1000

        -- 检查是否在防抖期内
        if current_time - last_trigger_time < debounce_interval then
            return  -- 跳过本次触发
        end

        -- 更新最后触发时间
        last_trigger_time = current_time

        print("Triggered (debounced)")
        -- 执行操作
    end
})
```

#### Python

```python
import time
from wingman import trigger

last_trigger_time = 0
debounce_interval = 1000  # 1 秒防抖

def debounced_action(match):
    global last_trigger_time
    current_time = time.time() * 1000

    # 检查是否在防抖期内
    if current_time - last_trigger_time < debounce_interval:
        return  # 跳过本次触发

    # 更新最后触发时间
    last_trigger_time = current_time

    print("Triggered (debounced)")

debounced_trigger = trigger.create({
    "type": "color",
    "color": 0xFF0000,
    "x": 0, "y": 0,
    "width": 1920, "height": 1080,
    "tolerance": 10,
    "action": debounced_action
})
```

### 触发计数

限制触发次数。

#### Lua

```lua
local trigger = require("wingman.trigger")

local max_triggers = 10
local trigger_count = 0

local limited_trigger = trigger.create({
    type = "image",
    image = "target.png",
    x = 0, y = 0,
    width = 1920, height = 1080,
    threshold = 0.8,

    action = function(match)
        -- 检查触发次数
        if trigger_count >= max_triggers then
            print("Max triggers reached, stopping")
            trigger.stop(limited_trigger)
            return
        end

        trigger_count = trigger_count + 1
        print(string.format("Trigger %d/%d", trigger_count, max_triggers))

        -- 执行操作
    end
})
```

#### Python

```python
from wingman import trigger

max_triggers = 10
trigger_count = 0

def limited_action(match):
    global trigger_count

    # 检查触发次数
    if trigger_count >= max_triggers:
        print("Max triggers reached, stopping")
        trigger.stop(limited_trigger)
        return

    trigger_count += 1
    print(f"Trigger {trigger_count}/{max_triggers}")

limited_trigger = trigger.create({
    "type": "image",
    "image": "target.png",
    "x": 0, "y": 0,
    "width": 1920, "height": 1080,
    "threshold": 0.8,
    "action": limited_action
})
```

---

## 实战案例

### 游戏自动打怪

#### Lua

```lua
local trigger = require("wingman.trigger")
local input = require("wingman.input")
local screen = require("wingman.screen")

-- 查找怪物
local monster_trigger = trigger.create({
    type = "image",
    image = "monster.png",
    x = 0, y = 0,
    width = 1920, height = 1080,
    threshold = 0.85,
    interval = 200,

    action = function(match)
        print(string.format("Monster found at: %d, %d", match.x, match.y))

        -- 点击攻击
        input.click(match.x, match.y, "left")

        -- 等待攻击完成
        task.delay(2000, function()
            -- 检查是否还有怪物
            local screenshot = screen.capture(0, 0, 1920, 1080)
            -- 继续查找...
        end)
    end
})

-- 监控血量
local health_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,  -- 红色表示低血量
    x = 150, y = 50,   -- 血条位置
    width = 100, height = 20,
    tolerance = 15,
    interval = 500,

    action = function(match)
        print("Low health! Using potion...")
        input.keyPress("1")  -- 使用药品快捷键
    end
})

-- 监控蓝量
local mana_trigger = trigger.create({
    type = "color",
    color = 0x0000FF,  -- 蓝色表示低蓝量
    x = 150, y = 80,   -- 蓝条位置
    width = 100, height = 20,
    tolerance = 15,
    interval = 500,

    action = function(match)
        print("Low mana! Using mana potion...")
        input.keyPress("2")  -- 使用蓝药快捷键
    end
})

-- 启动所有触发器
trigger.start(monster_trigger)
trigger.start(health_trigger)
trigger.start(mana_trigger)

print("Auto-grind started! Press F10 to stop")
```

### 自动收集掉落物

#### Lua

```lua
local trigger = require("wingman.trigger")
local input = require("wingman.input")

-- 要收集的物品
local items = {"gold.png", "gem.png", "potion.png", "equipment.png"}

local last_collected = {}
local collection_cooldown = 2000  -- 2 秒冷却

for _, item_image in ipairs(items) do
    local item_trigger = trigger.create({
        type = "image",
        image = item_image,
        x = 0, y = 0,
        width = 1920, height = 1080,
        threshold = 0.8,
        interval = 300,

        action = function(match)
            local current_time = os.time() * 1000
            local item_key = string.format("%s_%d_%d",
                item_image, match.x, match.y)

            -- 检查冷却时间
            if last_collected[item_key] and
               current_time - last_collected[item_key] < collection_cooldown then
                return  -- 冷却中，跳过
            end

            -- 更新最后收集时间
            last_collected[item_key] = current_time

            -- 收集物品
            print(string.format("Collecting %s at: %d, %d",
                item_image, match.x, match.y))

            -- 移动到物品位置
            input.move(match.x, match.y, 300)
            task.delay(300, function()
                -- 点击收集
                input.click(match.x, match.y, "left")
            end)
        end
    })

    trigger.start(item_trigger)
end
```

### 状态监控系统

#### Lua

```lua
local trigger = require("wingman.trigger")
local notify = require("wingman.notify")

-- 定期检查系统状态
local system_monitor = trigger.create({
    type = "time",
    interval = 10000,  -- 每 10 秒

    action = function()
        -- 检查 CPU 使用率
        local cpu_usage = getCpuUsage()
        print(string.format("CPU: %.1f%%", cpu_usage))

        if cpu_usage > 80 then
            notify.send("高 CPU 使用率", string.format("当前 CPU 使用率: %.1f%%", cpu_usage))
        end

        -- 检查内存使用
        local mem_usage = getMemoryUsage()
        print(string.format("Memory: %.1f%%", mem_usage))

        if mem_usage > 80 then
            notify.send("高内存使用", string.format("当前内存使用率: %.1f%%", mem_usage))
        end

        -- 检查网络连接
        if not isNetworkConnected() then
            notify.send("网络断开", "网络连接已断开")
        end
    end
})

-- 监控特定进程
local process_monitor = trigger.create({
    type = "time",
    interval = 5000,  -- 每 5 秒

    action = function()
        if not process.exists("target_process.exe") then
            notify.send("进程异常", "目标进程已关闭")
            -- 可以在这里添加恢复操作
        end
    end
})
```

### 智能挂机系统

#### Lua

```lua
local trigger = require("wingman.trigger")
local input = require("wingman.input")

local state = {
    fighting = false,
    low_health = false,
    inventory_full = false
}

-- 主循环触发器
local main_loop = trigger.create({
    type = "time",
    interval = 1000,  -- 每秒检查一次

    action = function()
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
                input.click(monster.x, monster.y, "left")
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
    end
})

-- 紧急情况处理
local emergency_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,  -- 极低血量
    x = 150, y = 50,
    width = 100, height = 20,
    tolerance = 10,
    interval = 200,

    action = function(match)
        print("Emergency! Using survival items...")
        -- 使用紧急逃生技能
        input.keyPress("F12")
        task.delay(500, function()
            input.keyPress("F1")  -- 使用回城卷轴
        end)
    end
})
```

---

## 最佳实践

### 1. 触发器命名和管理

```lua
-- 为触发器分配有意义的名称
local triggers = {
    monster_detector = nil,
    health_monitor = nil,
    mana_monitor = nil
}

-- 创建触发器时保存引用
triggers.monster_detector = trigger.create({...})
triggers.health_monitor = trigger.create({...})
triggers.mana_monitor = trigger.create({...})

-- 方便管理
function stopAllTriggers()
    for name, trig in pairs(triggers) do
        if trig then
            trigger.stop(trig)
            print(string.format("Stopped: %s", name))
        end
    end
end
```

### 2. 错误处理

```lua
local safe_trigger = trigger.create({
    type = "image",
    image = "target.png",
    x = 0, y = 0,
    width = 1920, height = 1080,
    threshold = 0.8,

    action = function(match)
        local ok, err = pcall(function()
            -- 可能出错的操作
            performComplexAction(match.x, match.y)
        end)

        if not ok then
            print("Error in trigger action:", err)
            -- 可以选择停止触发器或重试
        end
    end
})
```

### 3. 性能优化

```lua
-- 避免过于频繁的检查
local optimized_trigger = trigger.create({
    type = "color",
    color = 0xFF0000,
    x = 0, y = 0,
    width = 1920, height = 1080,
    tolerance = 10,
    interval = 500,  -- 合理的检查间隔（避免过高）
    action = function(match)
        -- 快速操作
    end
})

-- 减小搜索区域
local focused_trigger = trigger.create({
    type = "image",
    image = "target.png",
    -- 只搜索特定区域而不是全屏
    x = 800, y = 200,
    width = 320, height = 400,
    threshold = 0.8,
    action = function(match)
        -- 处理匹配
    end
})
```

### 4. 调试支持

```lua
local debug = require("wingman.debug")

local debug_trigger = trigger.create({
    type = "image",
    image = "target.png",
    x = 0, y = 0,
    width = 1920, height = 1080,
    threshold = 0.8,

    action = function(match)
        -- 输出调试信息
        debug.log(string.format("Match found: x=%d, y=%d, confidence=%.2f",
            match.x, match.y, match.confidence))

        -- 在调试模式下可以设置断点
        if isDebugEnabled() then
            debug.breakpoint()
        end

        -- 执行实际操作
        processMatch(match)
    end
})
```

---

## 🔗 相关文档

- [核心 API - Trigger](../api/core.md#trigger-触发器系统)
- [脚本 API - Task](../api/script.md#task-任务管理)
- [核心 API - Screen](../api/core.md#screen-屏幕操作)

---

**返回**: [文档首页](../README.md) | [使用指南](../README.md#使用指南)
