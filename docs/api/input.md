# API: wingman.input

输入模拟模块，提供鼠标和键盘输入的模拟功能，支持人性化模拟。

> **💡 提示**：查看 [数据类型参考](./types.md) 了解 API 中使用的各种对象和数据结构。

## 模块概述

input 模块用于模拟用户的鼠标和键盘操作，主要功能包括：

- **鼠标操作**：点击、移动、拖拽、滚动
- **键盘操作**：按键输入、文本输入、快捷键
- **人性化模拟**：可选的随机延迟和不规则输入，使自动化更像真人操作

### 坐标系统

使用屏幕坐标系统，左上角为原点 (0, 0)：
- `click(x, y)` - 点击指定坐标
- 坐标单位为像素

---

## 鼠标点击

### click(x, y, button?)

**说明**：在指定位置执行鼠标点击。

**函数签名**：

```python
click(x: int, y: int, button: str = "left") -> None
```

```lua
click(x: number, y: number, button: string = "left") -> nil
```

**参数**：
- `x, y` - 点击位置坐标
- `button` - 可选，鼠标按键：`"left"`（左键，默认）、`"right"`（右键）、`"middle"`（中键）

:::tabs

== Python

```python:line-numbers
from wingman import input

# 左键点击（默认）
input.click(100, 100)

# 右键点击
input.click(100, 100, "right")

# 中键点击
input.click(100, 100, "middle")
```

== Lua

```lua:line-numbers
local input = require("wingman.input")

-- 左键点击（默认）
input.click(100, 100)

-- 右键点击
input.click(100, 100, "right")

-- 中键点击
input.click(100, 100, "middle")
```

:::

---

## 鼠标移动

### move(x, y, smooth?)

**说明**：移动鼠标到指定位置。

**函数签名**：

```python
move(x: int, y: int, smooth: bool = False) -> None
```

```lua
move(x: number, y: number, smooth: boolean = false) -> nil
```

**参数**：
- `x, y` - 目标位置坐标
- `smooth` - 可选，是否平滑移动（默认 false）

**使用场景**：
- `smooth=False`：瞬间移动，适合快速操作
- `smooth=True`：平滑移动，更像真人操作

:::tabs

== Python

```python:line-numbers
from wingman import input

# 瞬间移动到 (500, 300)
input.move(500, 300)

# 平滑移动到 (500, 300)（更像真人）
input.move(500, 300, smooth=True)
```

== Lua

```lua:line-numbers
local input = require("wingman.input")

-- 瞬间移动到 (500, 300)
input.move(500, 300)

-- 平滑移动到 (500, 300)（更像真人）
input.move(500, 300, true)
```

:::

---

## 鼠标拖拽

### drag(from_x, from_y, to_x, to_y, duration?)

**说明**：按住鼠标左键从起点拖拽到终点。

**函数签名**：

```python
drag(from_x: int, from_y: int, to_x: int, to_y: int, duration: int = 500) -> None
```

```lua
drag(fromX: number, fromY: number, toX: number, toY: number, duration: number = 500) -> nil
```

**参数**：
- `from_x, from_y` - 起点坐标
- `to_x, to_y` - 终点坐标
- `duration` - 可选，拖拽持续时间（毫秒），默认 500

:::tabs

== Python

```python:line-numbers
from wingman import input

# 从 (100, 100) 拖拽到 (500, 300)，持续 500ms
input.drag(100, 100, 500, 300, 500)

# 快速拖拽（200ms）
input.drag(100, 100, 500, 300, 200)
```

== Lua

```lua:line-numbers
local input = require("wingman.input")

-- 从 (100, 100) 拖拽到 (500, 300)，持续 500ms
input.drag(100, 100, 500, 300, 500)

-- 快速拖拽（200ms）
input.drag(100, 100, 500, 300, 200)
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
local input = require("wingman.input")

-- 向上滚动一行
input.scroll(500, 300, 120)

-- 向下滚动一页
input.scroll(500, 300, -360)
```

:::

---

## 键盘输入

### send_keys(keys)

**说明**：发送键盘按键或组合键。

**函数签名**：

```python
send_keys(keys: str) -> None
```

```lua
sendKeys(keys: string) -> nil
```

**参数**：
- `keys` - 按键字符串

**常用按键**：
- 字母数字：直接输入，如 `"abc"`, `"123"`
- 特殊键：`"{ENTER}"`, `"{SPACE}"`, `"{TAB}"`, `"{ESC}"`, `"{DELETE}"`
- 组合键：`"^c"` (Ctrl+C), `"^v"` (Ctrl+V), `"%"` (Shift)
- 功能键：`"{F1}"` - `"{F12}"`

**修饰键符号**：
- `^` = Ctrl
- `%` = Shift
- `+` = Alt

:::tabs

== Python

```python:line-numbers
from wingman import input

# 输入文本
input.send_keys("hello world")

# 输入回车
input.send_keys("{ENTER}")

# 复制（Ctrl+C）
input.send_keys("^c")

# 粘贴（Ctrl+V）
input.send_keys("^v")

# 全选（Ctrl+A）
input.send_keys("^a")

# 删除选中内容
input.send_keys("{DELETE}")
```

== Lua

```lua:line-numbers
local input = require("wingman.input")

-- 输入文本
input.sendKeys("hello world")

-- 输入回车
input.sendKeys("{ENTER}")

-- 复制（Ctrl+C）
input.sendKeys("^c")

-- 粘贴（Ctrl+V）
input.sendKeys("^v")

-- 全选（Ctrl+A）
input.sendKeys("^a")

-- 删除选中内容
input.sendKeys("{DELETE}")
```

:::

---

## 获取鼠标位置

### get_mouse_pos() / getMousePos()

**说明**：获取当前鼠标位置。

**函数签名**：

```python
get_mouse_pos() -> tuple[int, int]
```

```lua
getMousePos() -> number, number
```

**返回**：
- Python: `(x, y)` 元组
- Lua: `x, y` 两个返回值

:::tabs

== Python

```python:line-numbers
from wingman import input

x, y = input.get_mouse_pos()
print(f"鼠标位置: {x}, {y}")
```

== Lua

```lua:line-numbers
local input = require("wingman.input")

local x, y = input.getMousePos()
print(string.format("鼠标位置: %d, %d", x, y))
```

:::

---

## 可用接口

### 鼠标操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `click(x, y, button?)` | `click(x, y, button?)` | 鼠标点击 | x,y: 坐标<br>button: "left"/"right"/"middle" |
| `move(x, y, smooth?)` | `move(x, y, smooth?)` | 移动鼠标 | x,y: 目标坐标<br>smooth: 是否平滑 |
| `drag(x1, y1, x2, y2, dur?)` | `drag(x1, y1, x2, y2, dur?)` | 鼠标拖拽 | x1,y1: 起点<br>x2,y2: 终点<br>dur: 持续时间 |
| `scroll(x, y, delta)` | `scroll(x, y, delta)` | 滚轮滚动 | x,y: 位置<br>delta: 滚动量 |
| `get_mouse_pos()` | `getMousePos()` | 获取鼠标位置 | 返回: x, y |

### 键盘操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `send_keys(keys)` | `sendKeys(keys)` | 发送按键 | keys: 按键字符串 |
