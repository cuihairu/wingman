> [中文](types.md) | **English**

# API Data Types Reference

This document describes the various data types and object structures used in the Wingman API.

## Table of Contents

- [Image Object](#image-object)
- [UIElement Object](#uielement-object)
- [Bounds Object](#bounds-object)
- [ElementInfo Object](#elementinfo-object)
- [HTTP Response Object](#http-response-object)
- [Image Match Result](#image-match-result)
- [Color Values](#color-values)
- [Process and Window Handles](#process-and-window-handles)

---

## Image Object

**Description**: There is no standalone Image object at the script layer. `screen.capture()` returns a **boolean** (whether the capture succeeded); the captured image data is not exposed to the script layer.

**Example**:

```python
from wingman import screen

# 截屏只返回是否成功（boolean）
ok = screen.capture()

# 图像匹配以图片文件路径为模板，不是内存图像对象
point, found = screen.find_image("button.png", {"x": 0, "y": 0, "width": 1920, "height": 1080})
```

**Note**: To get the color of a screen pixel, use `screen.get_pixel(x, y)`, which returns an `{r, g, b, a}` color object (see [Color Values](#color-values)).

---

## UIElement Object

> [The following 18 methods are implemented (Windows UIAutomation + macOS Accessibility, dual platform). UIElement is returned as a script object that references `IUIAElement` internally via a handle.]

**Source**: All lookup functions of the `uia` module

**Description**: Represents a UI Automation element (button, edit box, list, etc.). It is a **method table**: a plain table in Lua (method names as snake_case keys) and a `dict` in Python — there is **no attribute access**; you must fetch the method by dictionary key and then call it.

**Method table (identical key names on both ends, all snake_case)**:

| Key | Description |
|------|------|
| `get_info()` | Gets element info (returns an ElementInfo object) |
| `click()` | Clicks the element |
| `right_click()` | Right-clicks the element (macOS opens the context menu via `AXShowMenu`) |
| `double_click()` | Double-clicks the element |
| `focus()` | Sets focus |
| `get_value()` | Gets the element text (implemented as getText) |
| `set_value(value)` | Sets the element text (implemented as setText; only effective for text-capable controls) |
| `get_children()` | Gets the list of child elements |
| `get_parent()` | Gets the parent element (returns `None`/`nil` past the root) |
| `expand()` | Expands the element |
| `collapse()` | Collapses the element |
| `is_expanded()` | Checks whether it is expanded |
| `is_visible()` | Checks whether it is visible |
| `is_enabled()` | Checks whether it is enabled |
| `is_checked()` | Checks the checked state (Windows Toggle pattern / macOS AXChecked) |
| `set_checked(checked)` | Sets the checked state (`true`/`false`) |
| `select()` | Programmatically selects the element (list items/tree nodes/tab items etc.; no mouse movement) |
| `get_selection()` | Container query for the first selected child (returns `None`/`nil` when none) |

**Example**:

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
    -- 方法名为 snake_case 键（无 camelCase 别名），点号调用
    -- （冒号调用会把元素自身混入参数，带参方法如 set_value 会失效）
    local info = btn.get_info()
    btn.click()
    if btn.is_enabled() then
        print("按钮可用")
    end
end
```

---

## Bounds Object

**Source**: `window.get_bounds()`, `ElementInfo.bounds`

**Description**: Represents the position and size of a rectangular region.

**Python structure**:

```python
{
    "x": int,      # 左上角 X 坐标
    "y": int,      # 左上角 Y 坐标
    "width": int,  # 宽度
    "height": int  # 高度
}
```

**Lua structure**:

```lua
{
    x = number,      -- 左上角 X 坐标
    y = number,      -- 左上角 Y 坐标
    width = number,  -- 宽度
    height = number  -- 高度
}
```

**Example**:

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

## ElementInfo Object

**Source**: `UIElement.get_info()` (the `get_info` key in the method table)

**Description**: Contains all property information of a UI element. Key names are identical on both ends (no language-level conversion).

**Structure**:

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

The Lua structure uses the same key names as above (`name`/`id`/`className`/`role`/`text`/`is_enabled`/`is_visible`/`has_focus`/`bounds`).

**Example**:

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

## HTTP Response Object

**Source**: All request functions of the `http` module

**Description**: The result of an HTTP request.

**Python structure**:

```python
{
    "success": bool,    # 请求是否成功（状态码 2xx）
    "status": int,      # HTTP 状态码
    "body": str,        # 响应体内容
    "headers": dict     # 响应头
}
```

**Lua structure**:

```lua
{
    success = boolean,  -- 请求是否成功
    status = number,    -- HTTP 状态码
    body = string,      -- 响应体内容
    headers = table     -- 响应头
}
```

**Example**:

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

## Image Match Result

**Source**: `screen.find_image()` / `screen.findImage()`

**Description**: The image match result is a **two-element array** `[match point, found]`, not an object, and it has no confidence field.

**Python structure**: a `list` shaped like `[{"x": int, "y": int}, True]`; `[None, False]` when not found

**Lua structure**: a sequential table shaped like `{ {x=..., y=...}, true }` (`result[1]` is the point table, `result[2]` the boolean); `{ nil, false }` when not found

**Signature**: `find_image(image_path: str, region: dict = full screen, threshold: float = 0.9)`

**Example**:

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

## Color Values

**Source**: `screen.get_pixel()` (return value), `screen.find_color()` / `screen.findColors()` (input)

**Description**: Colors take two forms at the script layer:

- **The return value of `screen.get_pixel(x, y)`** is a color **object** `{r, g, b, a}` (four integer components in 0-255), not an integer.
- **The `0xRRGGBB` 24-bit integer** is used only as the **input** format for `find_color` / `findColors` (which also accept an `{r, g, b}` object).

**`0xRRGGBB` input format**:
- `RR` - Red component (0-255)
- `GG` - Green component (0-255)
- `BB` - Blue component (0-255)

**Common color values** (for `find_color` input):

| Color | Hex | Description |
|-----|---------|-----|
| Pure red | `0xFF0000` | |
| Pure green | `0x00FF00` | |
| Pure blue | `0x0000FF` | |
| White | `0xFFFFFF` | RGB(255, 255, 255) |
| Black | `0x000000` | RGB(0, 0, 0) |

**Color object returned by get_pixel**:

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

**Constructing find_color input**:

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

## Process and Window Handles

**Source**: `process.find()`, `window.find()`, `process.start()`

**Description**: Unique identifiers assigned by the operating system.

### Process ID (PID)

**Type**: integer

**Description**: Identifies a unique process in the system.

**Source**:
- The PID returned by `process.find()`
- `process.start()` returns the PID of the new process

**Example**:

```python
from wingman import process

pid, found = process.find("notepad")
if found:
    print(f"记事本 PID: {pid}")

# 启动新进程
new_pid = process.start("notepad.exe")
```

### Window Handle (HWND)

**Type**: integer

**Description**: Identifies a unique window in the system.

**Source**:
- The HWND returned by `window.find()`
- The HWND returned by `window.get_foreground()`

**Example**:

```python
from wingman import window

hwnd, found = window.find("记事本")
if found:
    print(f"记事本窗口句柄: {hwnd}")
    window.activate(hwnd)
```

---

## Related Documentation

- [API Index](./index.md) - A list of all API modules
- [wingman.screen](./screen.md) - Screen operations module
- [wingman.uia](./uia/) - UI Automation module
- [wingman.http](./http.md) - HTTP client module
- [wingman.process](./process.md) - Process management module
- [wingman.window](./window.md) - Window management module
