# API 数据类型参考

本文档详细说明了 Wingman API 中使用的各种数据类型和对象结构。

## 目录

- [Image 对象](#image-对象)
- [UIElement 对象](#uielement-对象)
- [Bounds 对象](#bounds-对象)
- [ElementInfo 对象](#elementinfo-对象)
- [HTTP Response 对象](#http-response-对象)
- [图像匹配结果](#图像匹配结果)
- [颜色值](#颜色值)
- [进程和窗口句柄](#进程和窗口句柄)

---

## Image 对象

**说明**：脚本层没有独立的 Image 图像对象。`screen.capture()` 返回的是 **boolean**（截图是否成功），截图数据不暴露给脚本层。

**示例**：

```python
from wingman import screen

# 截屏只返回是否成功（boolean）
ok = screen.capture()

# 图像匹配以图片文件路径为模板，不是内存图像对象
point, found = screen.find_image("button.png", {"x": 0, "y": 0, "width": 1920, "height": 1080})
```

**注意**：如需获取屏幕像素颜色，请使用 `screen.get_pixel(x, y)`，它返回 `{r, g, b, a}` 颜色对象（见[颜色值](#颜色值)）。

---

## UIElement 对象

> [以下 12 方法已实现（Windows UIAutomation + macOS Accessibility 双平台）。UIElement 以脚本对象形式返回，内部通过句柄引用 `IUIAElement`。]

**来源**：所有 `uia` 模块的查找函数

**说明**：代表一个 UI Automation 元素（按钮、编辑框、列表等）。它是一个**方法表**：Lua 中是普通 table（方法名为 snake_case 键），Python 中是 `dict`——**没有属性访问**，必须用字典键取出方法再调用。

**方法表（两端键名一致，均为 snake_case）**：

| 键名 | 说明 |
|------|------|
| `get_info()` | 获取元素信息（返回 ElementInfo 对象） |
| `click()` | 点击元素 |
| `double_click()` | 双击元素 |
| `focus()` | 设置焦点 |
| `get_value()` | 获取元素值 |
| `set_value(value)` | 设置元素值 |
| `get_children()` | 获取子元素列表 |
| `expand()` | 展开元素 |
| `collapse()` | 折叠元素 |
| `is_expanded()` | 检查是否已展开 |
| `is_visible()` | 检查是否可见 |
| `is_enabled()` | 检查是否可用 |

**示例**：

```python
from wingman import uia

btn = uia.find_button("确定")
if btn:
    # UIElement 是方法表字典：用键取出 Callable 再调用
    info = btn["get_info"]()

    # 点击元素
    btn["click"]()

    # 检查状态
    if btn["is_enabled"]():
        print("按钮可用")
```

```lua
local wingman = require("wingman")

local btn = wingman.uia.find_button("确定")
if btn then
    -- 方法名为 snake_case 键（无 camelCase 别名），冒号调用可用
    local info = btn:get_info()
    btn:click()
    if btn:is_enabled() then
        print("按钮可用")
    end
end
```

---

## Bounds 对象

**来源**：`window.get_bounds()`, `ElementInfo.bounds`

**说明**：表示矩形区域的位置和大小。

**Python 结构**：

```python
{
    "x": int,      # 左上角 X 坐标
    "y": int,      # 左上角 Y 坐标
    "width": int,  # 宽度
    "height": int  # 高度
}
```

**Lua 结构**：

```lua
{
    x = number,      -- 左上角 X 坐标
    y = number,      -- 左上角 Y 坐标
    width = number,  -- 宽度
    height = number  -- 高度
}
```

**示例**：

```python
from wingman import window

hwnd, found = window.find("记事本")
if found:
    bounds = window.get_bounds(hwnd)
    print(f"位置: ({bounds['x']}, {bounds['y']})")
    print(f"大小: {bounds['width']}x{bounds['height']}")

    # 计算中心点
    center_x = bounds['x'] + bounds['width'] // 2
    center_y = bounds['y'] + bounds['height'] // 2
```

---

## ElementInfo 对象

**来源**：`UIElement.get_info()`（即方法表中的 `get_info` 键）

**说明**：包含 UI 元素的所有属性信息。两端键名完全一致（无语言级转换）。

**结构**：

```python
{
    "name": str,        # 元素名称（显示文本）
    "id": str,          # AutomationId / 元素标识
    "className": str,   # 控件类名
    "role": int,        # 控件角色（整型枚举值，非字符串类型名）
    "text": str,        # 元素文本/值
    "is_enabled": bool,     # 是否可用
    "is_visible": bool,     # 是否可见
    "has_focus": bool,      # 是否持有焦点
    "bounds": {             # 边界矩形（Bounds 对象）
        "x": int,
        "y": int,
        "width": int,
        "height": int
    }
}
```

Lua 结构键名与上表一致（`name`/`id`/`className`/`role`/`text`/`is_enabled`/`is_visible`/`has_focus`/`bounds`）。

**示例**：

```python
from wingman import uia

btn = uia.find_button("确定")
if btn:
    info = btn["get_info"]()
    print(f"名称: {info['name']}")
    print(f"标识: {info['id']}")
    print(f"角色: {info['role']}")

    # 访问边界信息
    rect = info['bounds']
    print(f"位置: ({rect['x']}, {rect['y']})")
```

---

## HTTP Response 对象

**来源**：所有 `http` 模块的请求函数

**说明**：HTTP 请求的响应结果。

**Python 结构**：

```python
{
    "success": bool,    # 请求是否成功（状态码 2xx）
    "status": int,      # HTTP 状态码
    "body": str,        # 响应体内容
    "headers": dict     # 响应头
}
```

**Lua 结构**：

```lua
{
    success = boolean,  -- 请求是否成功
    status = number,    -- HTTP 状态码
    body = string,      -- 响应体内容
    headers = table     -- 响应头
}
```

**示例**：

```python
from wingman import http

resp = http.get("https://api.example.com/data")
if resp["success"]:
    print(f"状态码: {resp['status']}")
    print(f"响应内容: {resp['body']}")
else:
    print(f"请求失败: {resp['status']}")
```

---

## 图像匹配结果

**来源**：`screen.find_image()` / `screen.findImage()`

**说明**：图像匹配结果是一个**二元数组** `[匹配点, 是否找到]`，不是对象，也没有 confidence 字段。

**Python 结构**：`list`，形如 `[{"x": int, "y": int}, True]`；未找到时为 `[None, False]`

**Lua 结构**：顺序表，形如 `{ {x=..., y=...}, true }`（`result[1]` 为点表、`result[2]` 为布尔）；未找到时为 `{ nil, false }`

**签名**：`find_image(image_path: str, region: dict = 全屏, threshold: float = 0.9)`

**示例**：

```python
from wingman import screen

result = screen.find_image("button.png", {"x": 0, "y": 0, "width": 1920, "height": 1080})
point, found = result
if found:
    print(f"找到图像，位置: ({point['x']}, {point['y']})")
```

```lua
local wingman = require("wingman")

local result = wingman.screen.findImage("button.png", {x = 0, y = 0, width = 1920, height = 1080})
if result[2] then
    print(string.format("找到图像，位置: (%d, %d)", result[1].x, result[1].y))
end
```

---

## 颜色值

**来源**：`screen.get_pixel()`（返回）、`screen.find_color()` / `screen.findColors()`（输入）

**说明**：颜色在脚本层有两种形态：

- **`screen.get_pixel(x, y)` 的返回值**是颜色**对象** `{r, g, b, a}`（四个 0-255 整数分量），不是整数。
- **`0xRRGGBB` 24 位整数**只作为 `find_color` / `findColors` 的**输入**格式使用（也接受 `{r, g, b}` 对象）。

**`0xRRGGBB` 输入格式**：
- `RR` - 红色分量（0-255）
- `GG` - 绿色分量（0-255）
- `BB` - 蓝色分量（0-255）

**常见颜色值**（用于 `find_color` 输入）：

| 颜色 | 十六进制 | 说明 |
|-----|---------|-----|
| 纯红 | `0xFF0000` | |
| 纯绿 | `0x00FF00` | |
| 纯蓝 | `0x0000FF` | |
| 白色 | `0xFFFFFF` | RGB(255, 255, 255) |
| 黑色 | `0x000000` | RGB(0, 0, 0) |

**get_pixel 返回的颜色对象**：

```python
from wingman import screen

c = screen.get_pixel(100, 100)
print(c["r"], c["g"], c["b"], c["a"])   # 各分量 0-255
```

```lua
local wingman = require("wingman")

local c = wingman.screen.getPixel(100, 100)
print(c.r, c.g, c.b, c.a)
```

**构造 find_color 输入**：

```python
color = (r << 16) | (g << 8) | b   # 0xRRGGBB
# 或直接用对象
color = {"r": r, "g": g, "b": b}
```

```lua
local color = (r << 16) | (g << 8) | b
-- 或
local color = {r = r, g = g, b = b}
```

---

## 进程和窗口句柄

**来源**：`process.find()`, `window.find()`, `process.start()`

**说明**：操作系统分配的唯一标识符。

### 进程 ID（PID）

**类型**：整数

**说明**：标识系统中的唯一进程。

**来源**：
- `process.find()` 返回的 PID
- `process.start()` 返回新进程的 PID

**示例**：

```python
from wingman import process

pid, found = process.find("notepad")
if found:
    print(f"记事本 PID: {pid}")

# 启动新进程
new_pid = process.start("notepad.exe")
```

### 窗口句柄（HWND）

**类型**：整数

**说明**：标识系统中的唯一窗口。

**来源**：
- `window.find()` 返回的 HWND
- `window.get_foreground()` 返回的 HWND

**示例**：

```python
from wingman import window

hwnd, found = window.find("记事本")
if found:
    print(f"记事本窗口句柄: {hwnd}")
    window.activate(hwnd)
```

---

## 相关文档

- [API 索引](./index.md) - 所有 API 模块列表
- [wingman.screen](./screen.md) - 屏幕操作模块
- [wingman.uia](./uia/) - UI Automation 模块
- [wingman.http](./http.md) - HTTP 客户端模块
- [wingman.process](./process.md) - 进程管理模块
- [wingman.window](./window.md) - 窗口管理模块
