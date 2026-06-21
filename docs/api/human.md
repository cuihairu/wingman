# API: wingman.human

人性化模拟模块，为自动化操作添加随机性和自然感。

## 模块概述

human 模块提供人性化操作模拟功能：
- **随机延迟** - 在指定范围内随机延迟
- **平滑移动** - 使用贝塞尔曲线平滑移动鼠标
- **自然点击** - 模拟人类点击节奏
- **自然输入** - 模拟人类输入节奏
- **参数配置** - 设置延迟范围、移动速度、变异度

---

## 随机延迟

### random_delay(min?, max?) / randomDelay(min?, max?)

**说明**：在指定范围内随机延迟。

**函数签名**：

```python
random_delay(min: int = 100, max: int = 300) -> None
```

```lua
randomDelay(min: number = 100, max: number = 300) -> nil
```

**参数**：
- `min` - 可选，最小延迟（毫秒），默认 100
- `max` - 可选，最大延迟（毫秒），默认 300

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import human

# 随机延迟 50-150 毫秒
human.random_delay(50, 150)

# 使用默认延迟范围（100-300ms）
human.random_delay()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 随机延迟 50-150 毫秒
wingman.human.randomDelay(50, 150)

-- 使用默认延迟范围（100-300ms）
wingman.human.randomDelay()
```

:::

---

## 平滑移动鼠标

### move_mouse(x1, y1, x2, y2, duration?) / moveMouse(x1, y1, x2, y2, duration?)

**说明**：使用贝塞尔曲线平滑移动鼠标。

**函数签名**：

```python
move_mouse(x1: int, y1: int, x2: int, y2: int, duration: int = 500) -> None
```

```lua
moveMouse(x1: number, y1: number, x2: number, y2: number, duration: number = 500) -> nil
```

**参数**：
- `x1, y1` - 起始坐标
- `x2, y2` - 目标坐标
- `duration` - 可选，移动时长（毫秒），默认 500

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import human

# 使用贝塞尔曲线平滑移动鼠标
human.move_mouse(100, 100, 500, 300, duration=500)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 使用贝塞尔曲线平滑移动鼠标
wingman.human.moveMouse(100, 100, 500, 300, 500)
```

:::

---

## 模拟人类点击

### natural_click(x, y, button?) / naturalClick(x, y, button?)

**说明**：模拟人类点击节奏（包含随机延迟）。

**函数签名**：

```python
natural_click(x: int, y: int, button: str = "left") -> None
```

```lua
naturalClick(x: number, y: number, button: string = "left") -> nil
```

**参数**：
- `x, y` - 点击坐标
- `button` - 可选，按键类型，默认 `"left"`，可选 `"left"`, `"right"`, `"middle"`

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import human

# 模拟人类点击节奏（带随机延迟）
human.natural_click(100, 200)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 模拟人类点击节奏（带随机延迟）
wingman.human.naturalClick(100, 200)
```

:::

---

## 模拟人类输入

### natural_type(text) / naturalType(text)

**说明**：模拟人类输入节奏（包含随机按键延迟）。

**函数签名**：

```python
natural_type(text: str) -> None
```

```lua
naturalType(text: string) -> nil
```

**参数**：
- `text` - 要输入的文本

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import human

# 模拟人类输入节奏（带随机按键延迟）
human.natural_type("Hello World")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 模拟人类输入节奏（带随机按键延迟）
wingman.human.naturalType("Hello World")
```

:::

---

## 设置延迟范围

### set_delay_range(min, max) / setDelayRange(min, max)

**说明**：设置全局随机延迟范围。

**函数签名**：

```python
set_delay_range(min: int, max: int) -> None
```

```lua
setDelayRange(min: number, max: number) -> nil
```

**参数**：
- `min` - 最小延迟（毫秒）
- `max` - 最大延迟（毫秒）

**返回**：
- 无

---

## 设置移动速度

### set_move_speed(speed) / setMoveSpeed(speed)

**说明**：设置鼠标移动速度。

**函数签名**：

```python
set_move_speed(speed: float) -> None
```

```lua
setMoveSpeed(speed: number) -> nil
```

**参数**：
- `speed` - 速度系数（0.1-2.0），1.0 为正常速度

**返回**：
- 无

---

## 设置输入变异度

### set_typing_variance(variance) / setTypingVariance(variance)

**说明**：设置输入节奏变异度。

**函数签名**：

```python
set_typing_variance(variance: float) -> None
```

```lua
setTypingVariance(variance: number) -> nil
```

**参数**：
- `variance` - 变异度（0.0-1.0），0 为完全规律，1 为高度随机

**返回**：
- 无

---

## 设置参数

### set_config(key, value) / setConfig(key, value)

**说明**：设置单个配置参数。

**函数签名**：

```python
set_config(key: str, value: Any) -> None
```

```lua
setConfig(key: string, value: any) -> nil
```

**参数**：
- `key` - 参数名
- `value` - 参数值

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import human

# 设置全局随机延迟范围
human.set_delay_range(50, 200)

# 设置鼠标移动速度
human.set_move_speed(0.8)

# 设置输入节奏变异度
human.set_typing_variance(0.3)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 设置全局随机延迟范围
wingman.human.setDelayRange(50, 200)

-- 设置鼠标移动速度
wingman.human.setMoveSpeed(0.8)

-- 设置输入节奏变异度
wingman.human.setTypingVariance(0.3)
```

:::

---

## 获取配置

### get_config() / getConfig()

**说明**：获取当前人性化配置。

**函数签名**：

```python
get_config() -> dict
```

```lua
getConfig() -> table
```

**返回**：
- Python: 配置字典，包含 `delay_min`, `delay_max`, `move_speed`, `typing_variance` 等字段
- Lua: 配置表格

:::tabs

== Python

```python:line-numbers
from wingman import human

config = human.get_config()
print(f"延迟范围: {config['delay_min']}-{config['delay_max']}ms")
print(f"移动速度: {config['move_speed']}")
print(f"输入变异度: {config['typing_variance']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local config = wingman.human.getConfig()
print("延迟范围: " .. config.delay_min .. "-" .. config.delay_max .. "ms")
print("移动速度: " .. config.move_speed)
print("输入变异度: " .. config.typing_variance)
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `random_delay(min?, max?)` | `randomDelay(min?, max?)` | 随机延迟 | min: 最小延迟(默认100)<br>max: 最大延迟(默认300) |
| `move_mouse(x1, y1, x2, y2, duration?)` | `moveMouse(x1, y1, x2, y2, duration?)` | 平滑移动鼠标 | x1, y1: 起始坐标<br>x2, y2: 目标坐标<br>duration: 时长(默认500) |
| `natural_click(x, y, button?)` | `naturalClick(x, y, button?)` | 模拟人类点击 | x, y: 点击坐标<br>button: 按键类型(默认left) |
| `natural_type(text)` | `naturalType(text)` | 模拟人类输入 | text: 要输入的文本 |
| `set_delay_range(min, max)` | `setDelayRange(min, max)` | 设置延迟范围 | min: 最小延迟<br>max: 最大延迟 |
| `set_move_speed(speed)` | `setMoveSpeed(speed)` | 设置移动速度 | speed: 速度系数(0.1-2.0) |
| `set_typing_variance(variance)` | `setTypingVariance(variance)` | 设置输入变异度 | variance: 变异度(0.0-1.0) |
| `set_config(key, value)` | `setConfig(key, value)` | 设置配置参数 | key: 参数名<br>value: 参数值 |
| `get_config()` | `getConfig()` | 获取配置 | 返回: 配置对象 |
