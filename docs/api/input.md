# API: wingman.input

输入模拟模块。

## 函数

### click(x, y, button)

在指定位置点击鼠标。

**参数：**
- `x` (number) - X 坐标
- `y` (number) - Y 坐标
- `button` (string) - 按键类型，可选值：`"left"`, `"right"`, `"middle"` (默认 `"left"`)

**示例：**
```lua
local input = require("wingman.input")

-- 左键点击
input.click(100, 100)

-- 右键点击
input.click(100, 100, "right")

-- 中键点击
input.click(100, 100, "middle")
```

### move(x, y, smooth)

移动鼠标到指定位置。

**参数：**
- `x` (number) - 目标 X 坐标
- `y` (number) - 目标 Y 坐标
- `smooth` (boolean) - 是否平滑移动 (默认 false)

**示例：**
```lua
-- 瞬间移动
input.move(500, 300)

-- 平滑移动
input.move(500, 300, true)
```

### drag(x1, y1, x2, y2, duration)

拖拽鼠标。

**参数：**
- `x1, y1` (number) - 起始位置
- `x2, y2` (number) - 结束位置
- `duration` (number) - 拖拽时长（毫秒）

**示例：**
```lua
-- 拖拽 500ms
input.drag(100, 100, 500, 300, 500)
```

### keyDown(key)

按下按键。

**参数：**
- `key` (string) - 按键名称（虚拟码或名称）

**支持的按键名：**
- 字母：`"A"` ~ `"Z"`
- 数字：`"0"` ~ `"9"`
- 功能键：`"F1"` ~ `"F12"`
- 特殊键：`"SPACE"`, `"ENTER"`, `"ESC"`, `"TAB"`, `"SHIFT"`, `"CTRL"`, `"ALT"`
- 方向键：`"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`

**示例：**
```lua
input.keyDown("W")
-- ... 做一些操作
input.keyUp("W")
```

### keyUp(key)

释放按键。

**参数：**
- `key` (string) - 按键名称

**示例：**
```lua
input.keyUp("W")
```

### keyPress(key, duration)

按键（按下后立即释放）。

**参数：**
- `key` (string) - 按键名称
- `duration` (number) - 按键时长（毫秒），默认 50

**示例：**
```lua
-- 普通按键
input.keyPress("SPACE")

-- 长按 200ms
input.keyPress("E", 200)
```

### keyText(text)

输入文本。

**参数：**
- `text` (string) - 要输入的文本

**示例：**
```lua
input.keyText("Hello World")
```
