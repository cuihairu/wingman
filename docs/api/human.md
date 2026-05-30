# API: wingman.human

人性化模拟模块，为自动化操作添加随机性和自然感。

## 随机延迟

::: code-group

```python [Python]
from wingman import human

# 随机延迟 50-150 毫秒
human.random_delay(50, 150)

# 使用默认延迟范围（100-300ms）
human.random_delay()
```

```lua [Lua]
local human = require("wingman.human")

-- 随机延迟 50-150 毫秒
human.randomDelay(50, 150)

-- 使用默认延迟范围（100-300ms）
human.randomDelay()
```

:::

## 贝塞尔曲线鼠标移动

::: code-group

```python [Python]
from wingman import human, input

# 使用贝塞尔曲线平滑移动鼠标
human.move_mouse(100, 100, 500, 300, duration=500)
```

```lua [Lua]
local human = require("wingman.human")
local input = require("wingman.input")

-- 使用贝塞尔曲线平滑移动鼠标
human.moveMouse(100, 100, 500, 300, 500)
```

:::

## 模拟人类点击节奏

::: code-group

```python [Python]
from wingman import human, input

# 模拟人类点击节奏（带随机延迟）
human.natural_click(100, 200)
```

```lua [Lua]
local human = require("wingman.human")
local input = require("wingman.input")

-- 模拟人类点击节奏（带随机延迟）
human.naturalClick(100, 200)
```

:::

## 模拟人类输入节奏

::: code-group

```python [Python]
from wingman import human, input

# 模拟人类输入节奏（带随机按键延迟）
human.natural_type("Hello World")
```

```lua [Lua]
local human = require("wingman.human")
local input = require("wingman.input")

-- 模拟人类输入节奏（带随机按键延迟）
human.naturalType("Hello World")
```

:::

## 设置人性化参数

::: code-group

```python [Python]
from wingman import human

# 设置全局随机延迟范围
human.set_delay_range(50, 200)

# 设置鼠标移动速度
human.set_move_speed(0.8)

# 设置输入节奏变异度
human.set_typing_variance(0.3)
```

```lua [Lua]
local human = require("wingman.human")

-- 设置全局随机延迟范围
human.setDelayRange(50, 200)

-- 设置鼠标移动速度
human.setMoveSpeed(0.8)

-- 设置输入节奏变异度
human.setTypingVariance(0.3)
```

:::

## 获取当前配置

::: code-group

```python [Python]
from wingman import human

config = human.get_config()
print(f"延迟范围: {config['delay_min']}-{config['delay_max']}ms")
print(f"移动速度: {config['move_speed']}")
print(f"输入变异度: {config['typing_variance']}")
```

```lua [Lua]
local human = require("wingman.human")

local config = human.getConfig()
print("延迟范围: " .. config.delay_min .. "-" .. config.delay_max .. "ms")
print("移动速度: " .. config.move_speed)
print("输入变异度: " .. config.typing_variance)
```

:::

---

## 完整示例

::: code-group

```python [Python]
from wingman import human, input, screen

# 设置人性化参数
human.set_delay_range(80, 200)
human.set_move_speed(0.7)

# 模拟人类操作流程
# 1. 随机延迟
human.random_delay()

# 2. 平滑移动到按钮
human.move_mouse(400, 300, 450, 350, duration=400)
human.random_delay(50, 100)

# 3. 自然点击
human.natural_click(450, 350)
human.random_delay()

# 4. 输入文本（带随机节奏）
human.natural_type("player123")
human.random_delay(100, 200)

# 5. 点击登录按钮
human.natural_click(450, 400)
```

```lua [Lua]
local human = require("wingman.human")
local input = require("wingman.input")

-- 设置人性化参数
human.setDelayRange(80, 200)
human.setMoveSpeed(0.7)

-- 模拟人类操作流程
-- 1. 随机延迟
human.randomDelay()

-- 2. 平滑移动到按钮
human.moveMouse(400, 300, 450, 350, 500)
human.randomDelay(50, 100)

-- 3. 自然点击
human.naturalClick(450, 350)
human.randomDelay()

-- 4. 输入文本（带随机节奏）
human.naturalType("player123")
human.randomDelay(100, 200)

-- 5. 点击登录按钮
human.naturalClick(450, 400)
```

:::

---

## 可用接口

### `random_delay(min?, max?)` / `randomDelay(min?, max?)`

随机延迟。

**参数：**
- `min` - 最小延迟（毫秒），默认 100
- `max` - 最大延迟（毫秒），默认 300

### `move_mouse(x1, y1, x2, y2, duration?)` / `moveMouse(x1, y1, x2, y2, duration?)`

使用贝塞尔曲线平滑移动鼠标。

**参数：**
- `x1, y1` - 起始坐标
- `x2, y2` - 目标坐标
- `duration` - 移动时长（毫秒），默认 500

### `natural_click(x, y, button?)` / `naturalClick(x, y, button?)`

模拟人类点击节奏。

**参数：**
- `x, y` - 点击坐标
- `button` - 按键类型，默认 `"left"`

### `natural_type(text)` / `naturalType(text)`

模拟人类输入节奏。

**参数：**
- `text` - 要输入的文本

### `set_delay_range(min, max)` / `setDelayRange(min, max)`

设置全局随机延迟范围。

**参数：**
- `min` - 最小延迟（毫秒）
- `max` - 最大延迟（毫秒）

### `set_move_speed(speed)` / `setMoveSpeed(speed)`

设置鼠标移动速度。

**参数：**
- `speed` - 速度系数（0.1-2.0），1.0 为正常速度

### `set_typing_variance(variance)` / `setTypingVariance(variance)`

设置输入节奏变异度。

**参数：**
- `variance` - 变异度（0.0-1.0），0 为完全规律，1 为高度随机

### `get_config()` / `getConfig()`

获取当前人性化配置。

**返回：**
- `dict/table` - 包含当前配置参数
