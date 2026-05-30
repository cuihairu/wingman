# Notify API

`wingman.notify` 提供统一的通知与告警出口，支持日志、toast、webhook 和事件桥接。

## 日志通知

:::tabs

== Python

```python:line-numbers
from wingman import notify

# 各级别日志
notify.debug("调试信息", {"module": "combat"})
notify.info("脚本启动", {"script": "farm.py"})
notify.warn("资源不足", {"hp": 10})
notify.error("任务失败", {"error": "timeout"})
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- 各级别日志
notify.debug("调试信息", { module = "combat" })
notify.info("脚本启动", { script = "farm.lua" })
notify.warn("资源不足", { hp = 10 })
notify.error("任务失败", { error = "timeout" })
```

:::

## Toast 通知

:::tabs

== Python

```python:line-numbers
from wingman import notify

# Toast 通知
notify.toast("Wingman", "任务完成", level="success")
notify.toast("警告", "血量过低", level="warning")
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- Toast 通知
notify.toast("Wingman", "任务完成", "success")
notify.toast("警告", "血量过低", "warning")
```

:::

## Webhook

:::tabs

== Python

```python:line-numbers
from wingman import notify

# Webhook
notify.webhook("http://127.0.0.1:9000/hook", {
    "event": "task.done",
    "result": 42
})
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- Webhook
notify.webhook("http://127.0.0.1:9000/hook", {
    event = "task.done",
    result = 42
})
```

:::

## 事件桥接

:::tabs

== Python

```python:line-numbers
from wingman import notify

# 事件桥接
notify.bridge("combat.*", "event://logging.combat_events")
notify.bridge("task.failed", "http://127.0.0.1:9000/alert")
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- 事件桥接
notify.bridge("combat.*", "event://logging.combat_events")
notify.bridge("task.failed", "http://127.0.0.1:9000/alert")
```

:::

---

## 完整示例

### 游戏监控通知系统

:::tabs

== Python

```python:line-numbers
from wingman import notify, vision, util

def check_game_status():
    while True:
        # 检查血量
        hp = vision.get_hp()
        if hp < 30:
            notify.warn("血量过低", {"hp": hp})
            notify.toast("警告", f"血量仅剩 {hp}", level="warning")

        # 检查蓝量
        mp = vision.get_mp()
        if mp < 10:
            notify.error("蓝量耗尽", {"mp": mp})
            notify.toast("危险", "蓝量已耗尽", level="error")

        # 检查背包
        inventory_full = vision.check_inventory_full()
        if inventory_full:
            notify.info("背包已满", {"slots": 0})
            notify.toast("提示", "背包已满，请清理", level="info")

        util.sleep(5000)

check_game_status()
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")
local vision = require("wingman.vision")
local util = require("wingman.util")

local function checkGameStatus()
    while true do
        -- 检查血量
        local hp = vision.getHp()
        if hp < 30 then
            notify.warn("血量过低", { hp = hp })
            notify.toast("警告", "血量仅剩 " .. hp, "warning")
        end

        -- 检查蓝量
        local mp = vision.getMp()
        if mp < 10 then
            notify.error("蓝量耗尽", { mp = mp })
            notify.toast("危险", "蓝量已耗尽", "error")
        end

        -- 检查背包
        local inventoryFull = vision.checkInventoryFull()
        if inventoryFull then
            notify.info("背包已满", { slots = 0 })
            notify.toast("提示", "背包已满，请清理", "info")
        end

        util.sleep(5000)
    end
end

checkGameStatus()
```

:::

### Webhook 远程通知

:::tabs

== Python

```python:line-numbers
from wingman import notify, vision, input

def combat_bot():
    notify.info("战斗脚本启动", {"mode": "auto"})

    kills = 0
    while True:
        # 查找敌人
        enemy = vision.find_enemy()
        if enemy:
            # 战斗逻辑
            input.click(enemy['x'], enemy['y'])
            kills += 1

            # 每10杀通知一次
            if kills % 10 == 0:
                notify.webhook("http://your-server.com/api/progress", {
                    "event": "milestone",
                    "kills": kills,
                    "level": vision.get_level()
                })
                notify.toast("进度", f"已击杀 {kills} 个敌人", level="info")

        util.sleep(1000)

    notify.info("战斗脚本结束", {"total_kills": kills})

combat_bot()
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")
local vision = require("wingman.vision")
local input = require("wingman.input")
local util = require("wingman.util")

local function combatBot()
    notify.info("战斗脚本启动", { mode = "auto" })

    local kills = 0
    while true do
        -- 查找敌人
        local enemy = vision.findEnemy()
        if enemy then
            -- 战斗逻辑
            input.click(enemy.x, enemy.y)
            kills = kills + 1

            -- 每10杀通知一次
            if kills % 10 == 0 then
                notify.webhook("http://your-server.com/api/progress", {
                    event = "milestone",
                    kills = kills,
                    level = vision.getLevel()
                })
                notify.toast("进度", "已击杀 " .. kills .. " 个敌人", "info")
            end
        end

        util.sleep(1000)
    end

    notify.info("战斗脚本结束", { total_kills = kills })
end

combatBot()
```

:::

### 事件桥接集成

:::tabs

== Python

```python:line-numbers
from wingman import notify, event

# 设置事件桥接
notify.bridge("combat.*", "event://logging.combat_events")
notify.bridge("system.*", "http://monitoring-server.com/api/events")

# 现在所有匹配的事件都会自动转发
event.emit("combat.enemy_found", {"x": 100, "y": 200})  # 自动桥接到日志系统
event.emit("system.low_memory", {"available_mb": 512})   # 自动 POST 到监控服务器

# 也可以桥接特定事件
notify.bridge("task.error", "http://alert-server.com/api/alerts")
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")
local event = require("wingman.event")

-- 设置事件桥接
notify.bridge("combat.*", "event://logging.combat_events")
notify.bridge("system.*", "http://monitoring-server.com/api/events")

-- 现在所有匹配的事件都会自动转发
event.emit("combat.enemy_found", { x = 100, y = 200 })  -- 自动桥接到日志系统
event.emit("system.low_memory", { available_mb = 512 })   -- 自动 POST 到监控服务器

-- 也可以桥接特定事件
notify.bridge("task.error", "http://alert-server.com/api/alerts")
```

:::

---

## 可用接口

### `debug(message, meta?)` / `debug(message, meta?)`

输出调试级别日志。

**参数：**
- `message` - 日志消息
- `meta` - 可选，元数据对象

### `info(message, meta?)` / `info(message, meta?)`

输出信息级别日志。

**参数：**
- `message` - 日志消息
- `meta` - 可选，元数据对象

### `warn(message, meta?)` / `warn(message, meta?)`

输出警告级别日志。

**参数：**
- `message` - 日志消息
- `meta` - 可选，元数据对象

### `error(message, meta?)` / `error(message, meta?)`

输出错误级别日志。

**参数：**
- `message` - 日志消息
- `meta` - 可选，元数据对象

### `toast(title, message, level?)` / `toast(title, message, level?)`

显示桌面 toast 通知。

**参数：**
- `title` - 标题
- `message` - 消息内容
- `level` - 可选，级别：`"info"`, `"success"`, `"warning"`, `"error"`，默认 `"info"`

### `webhook(url, payload, options?)` / `webhook(url, payload, options?)`

发送 HTTP POST webhook。

**参数：**
- `url` - 目标 URL
- `payload` - 请求体（JSON 对象）
- `options` - 可选配置（预留）

### `bridge(eventName, target, options?)` / `bridge(eventName, target, options?)`

桥接事件到其他目标。

**参数：**
- `eventName` - 事件名称（精确匹配，暂不支持通配符）
- `target` - 目标地址（支持 `event://` 协议或 HTTP URL）
- `options` - 可选配置（预留）

**注意：** 当前实现仅支持精确匹配，不支持通配符模式。如需监听多个事件，请多次调用 `bridge()`。
