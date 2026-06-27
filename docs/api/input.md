# API: wingman.input

输入模拟模块，提供鼠标和键盘输入的模拟功能。

> **💡 提示**：查看 [数据类型参考](./types.md) 了解 API 中使用的各种对象和数据结构。

## 模块概述

input 模块用于模拟用户的鼠标和键盘操作，主要功能包括：

- **鼠标操作**：点击、移动、滚轮滚动
- **键盘操作**：按键输入、文本输入
- **延迟控制**：固定延迟、随机延迟

### 坐标系统

使用屏幕坐标系统，左上角为原点 (0, 0)：
- `click(x, y, button)` - 点击指定坐标
- 坐标单位为像素

---

## 鼠标点击

### click(x, y, button)

**说明**：在指定位置执行鼠标点击。

**函数签名**：

```python
click(x: int, y: int, button: int) -> None
```

```lua
click(x: number, y: number, button: number) -> nil
```

**参数**：
- `x, y` - 点击位置坐标
- `button` - 鼠标按键（整数）：
  - `0` - 左键
  - `1` - 右键
  - `2` - 中键

:::tabs

== Python

```python:line-numbers
from wingman import input

# 左键点击
input.click(100, 100, 0)

# 右键点击
input.click(100, 100, 1)

# 中键点击
input.click(100, 100, 2)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 左键点击
wingman.input.click(100, 100, 0)

-- 右键点击
wingman.input.click(100, 100, 1)

-- 中键点击
wingman.input.click(100, 100, 2)
```

:::

---

## 鼠标移动

### move(x, y, duration)

**说明**：移动鼠标到指定位置。

**函数签名**：

```python
move(x: int, y: int, duration: int) -> None
```

```lua
move(x: number, y: number, duration: number) -> nil
```

**参数**：
- `x, y` - 目标位置坐标
- `duration` - 移动耗时（毫秒）

**使用场景**：
- `duration` 较小：快速移动，适合瞬移操作
- `duration` 较大：缓慢移动，更像真人操作

:::tabs

== Python

```python:line-numbers
from wingman import input

# 快速移动到 (500, 300)
input.move(500, 300, 0)

# 用 300ms 平滑移动到 (500, 300)（更像真人）
input.move(500, 300, 300)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 快速移动到 (500, 300)
wingman.input.move(500, 300, 0)

-- 用 300ms 平滑移动到 (500, 300)（更像真人）
wingman.input.move(500, 300, 300)
```

:::

---

## 滚轮滚动

### scroll(x, y, delta)

**说明**：在指定位置执行滚轮滚动。

**函数签名**：

```python
scroll(x: int, y: int, delta: int) -> None
```

```lua
scroll(x: number, y: number, delta: number) -> nil
```

**参数**：
- `x, y` - 滚动位置坐标
- `delta` - 滚动量（正数向上，负数向下）

**滚动量参考**：
- 120 - 通常为一行
- 360 - 通常为一页
- -120 / -360 - 向下滚动

:::tabs

== Python

```python:line-numbers
from wingman import input

# 向上滚动一行
input.scroll(500, 300, 120)

# 向下滚动一页
input.scroll(500, 300, -360)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 向上滚动一行
wingman.input.scroll(500, 300, 120)

-- 向下滚动一页
wingman.input.scroll(500, 300, -360)
```

:::

---

## 键盘按键

### key(vkCode)

**说明**：模拟一次完整的按键（按下并抬起）。

**函数签名**：

```python
key(vkCode: int) -> None
```

```lua
key(vkCode: number) -> nil
```

**参数**：
- `vkCode` - 虚拟键码（Windows 虚拟键码，整数）

:::tabs

== Python

```python:line-numbers
from wingman import input

# 按下回车键（VK_RETURN = 0x0D = 13）
input.key(0x0D)

# 按下空格键（VK_SPACE = 0x20 = 32）
input.key(0x20)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 按下回车键（VK_RETURN = 0x0D = 13）
wingman.input.key(0x0D)

-- 按下空格键（VK_SPACE = 0x20 = 32）
wingman.input.key(0x20)
```

:::

---

## 按键按下 / 抬起

### keyDown(vkCode) / keyUp(vkCode)

**说明**：模拟按键按下或抬起，配合使用可实现组合键。

**函数签名**：

```python
keyDown(vkCode: int) -> None
keyUp(vkCode: int) -> None
```

```lua
keyDown(vkCode: number) -> nil
keyUp(vkCode: number) -> nil
```

**参数**：
- `vkCode` - 虚拟键码（Windows 虚拟键码，整数）

:::tabs

== Python

```python:line-numbers
from wingman import input

# 组合键 Ctrl+C（VK_CONTROL = 0x11，'C' = 0x43）
input.keyDown(0x11)  # 按下 Ctrl
input.keyDown(0x43)  # 按下 C
input.keyUp(0x43)    # 抬起 C
input.keyUp(0x11)    # 抬起 Ctrl
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 组合键 Ctrl+C（VK_CONTROL = 0x11，'C' = 0x43）
wingman.input.keyDown(0x11)  -- 按下 Ctrl
wingman.input.keyDown(0x43)  -- 按下 C
wingman.input.keyUp(0x43)    -- 抬起 C
wingman.input.keyUp(0x11)    -- 抬起 Ctrl
```

:::

---

## 文本输入

### type(text, delay?)

**说明**：输入一段文本，逐字符模拟。

**函数签名**：

```python
type(text: str, delay: int = 0) -> None
```

```lua
type(text: string, delay: number = 0) -> nil
```

**参数**：
- `text` - 要输入的文本
- `delay` - 可选，每个字符之间的间隔（毫秒），默认 0

:::tabs

== Python

```python:line-numbers
from wingman import input

# 输入文本
input.type("hello world")

# 慢速输入，每个字符间隔 50ms
input.type("hello world", 50)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 输入文本
wingman.input.type("hello world")

-- 慢速输入，每个字符间隔 50ms
wingman.input.type("hello world", 50)
```

:::

---

## 延迟

### delay(ms)

**说明**：让脚本暂停指定的毫秒数。

**函数签名**：

```python
delay(ms: int) -> None
```

```lua
delay(ms: number) -> nil
```

**参数**：
- `ms` - 暂停时长（毫秒）

:::tabs

== Python

```python:line-numbers
from wingman import input

# 暂停 1000ms（1 秒）
input.delay(1000)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 暂停 1000ms（1 秒）
wingman.input.delay(1000)
```

:::

---

## 随机延迟

### randomDelay(min, max)

**说明**：在 `[min, max]` 范围内随机暂停一段时间，使自动化更像真人操作。

**函数签名**：

```python
randomDelay(min: int, max: int) -> None
```

```lua
randomDelay(min: number, max: number) -> nil
```

**参数**：
- `min` - 最小暂停时长（毫秒）
- `max` - 最大暂停时长（毫秒）

:::tabs

== Python

```python:line-numbers
from wingman import input

# 随机暂停 500ms 到 1500ms
input.randomDelay(500, 1500)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 随机暂停 500ms 到 1500ms
wingman.input.randomDelay(500, 1500)
```

:::

---

## 可用接口

### 鼠标操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `click(x, y, button)` | `click(x, y, button)` | 鼠标点击 | x,y: 坐标<br>button: 0=左/1=右/2=中 |
| `move(x, y, duration)` | `move(x, y, duration)` | 移动鼠标 | x,y: 目标坐标<br>duration: 移动耗时(ms) |
| `scroll(x, y, delta)` | `scroll(x, y, delta)` | 滚轮滚动 | x,y: 位置<br>delta: 滚动量 |

### 键盘操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `key(vkCode)` | `key(vkCode)` | 按键（按下并抬起） | vkCode: 虚拟键码 |
| `keyDown(vkCode)` | `keyDown(vkCode)` | 按键按下 | vkCode: 虚拟键码 |
| `keyUp(vkCode)` | `keyUp(vkCode)` | 按键抬起 | vkCode: 虚拟键码 |
| `type(text, delay?)` | `type(text, delay?)` | 输入文本 | text: 文本<br>delay: 字符间隔(ms) |

### 延迟

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `delay(ms)` | `delay(ms)` | 固定延迟 | ms: 毫秒 |
| `randomDelay(min, max)` | `randomDelay(min, max)` | 随机延迟 | min,max: 毫秒范围 |
