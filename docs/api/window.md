# API: wingman.window

窗口管理模块。

## 查找窗口

::: code-group

```python [Python]
from wingman import window

hwnd, found = window.find("记事本")
if found:
    print(f"找到记事本窗口，句柄: {hwnd}")
```

```lua [Lua]
local window = require("wingman.window")

local hwnd, found = window.find("记事本")
if found then
    print("找到记事本窗口，句柄: " .. hwnd)
end
```

:::

## 激活窗口

::: code-group

```python [Python]
from wingman import window

hwnd, found = window.find("记事本")
if found:
    window.activate(hwnd)
```

```lua [Lua]
local window = require("wingman.window")

local hwnd, found = window.find("记事本")
if found then
    window.activate(hwnd)
end
```

:::

## 获取前台窗口

::: code-group

```python [Python]
from wingman import window

hwnd = window.get_foreground()
title = window.get_title(hwnd)
print(f"前台窗口: {title}")
```

```lua [Lua]
local window = require("wingman.window")

local hwnd = window.getForeground()
local title = window.getTitle(hwnd)
print("前台窗口: " .. title)
```

:::

## 获取窗口标题

::: code-group

```python [Python]
from wingman import window

hwnd = window.get_foreground()
title = window.get_title(hwnd)
print(f"标题: {title}")
```

```lua [Lua]
local window = require("wingman.window")

local hwnd = window.getForeground()
local title = window.getTitle(hwnd)
print("标题: " .. title)
```

:::

## 获取窗口边界

::: code-group

```python [Python]
from wingman import window

hwnd, found = window.find("记事本")
if found:
    bounds = window.get_bounds(hwnd)
    print(f"位置: ({bounds['x']}, {bounds['y']}), 大小: {bounds['width']}x{bounds['height']}")
```

```lua [Lua]
local window = require("wingman.window")

local hwnd, found = window.find("记事本")
if found then
    local bounds = window.getBounds(hwnd)
    print(string.format("位置: (%d, %d), 大小: %dx%d",
        bounds.x, bounds.y, bounds.width, bounds.height))
end
```

:::

## 等待窗口出现

::: code-group

```python [Python]
from wingman import window, process

# 启动应用程序
process.start("notepad.exe")

# 等待窗口出现
if window.wait_for("记事本", 5000):
    print("记事本已启动")
    hwnd, found = window.find("记事本")
    if found:
        window.activate(hwnd)
else:
    print("超时：记事本未启动")
```

```lua [Lua]
local window = require("wingman.window")
local process = require("wingman.process")

-- 启动应用程序
process.start("notepad.exe")

-- 等待窗口出现
if window.waitFor("记事本", 5000) then
    print("记事本已启动")
    local hwnd, found = window.find("记事本")
    if found then
        window.activate(hwnd)
    end
else
    print("超时：记事本未启动")
end
```

:::

---

## 完整示例

::: code-group

```python [Python]
from wingman import window, process, util, input

# 查找并激活记事本
hwnd, found = window.find("记事本")
if not found:
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")

    # 等待窗口出现
    if not window.wait_for("记事本", 3000):
        print("启动失败")
        exit(1)

    hwnd, found = window.find("记事本")

if found:
    # 激活窗口
    window.activate(hwnd)
    util.sleep(200)

    # 获取窗口信息
    title = window.get_title(hwnd)
    print(f"标题: {title}")

    bounds = window.get_bounds(hwnd)
    print(f"位置: ({bounds['x']}, {bounds['y']})")
    print(f"大小: {bounds['width']}x{bounds['height']}")
```

```lua [Lua]
local window = require("wingman.window")
local process = require("wingman.process")
local util = require("wingman.util")

-- 查找并激活记事本
local hwnd, found = window.find("记事本")
if not found then
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")

    -- 等待窗口出现
    if not window.waitFor("记事本", 3000) then
        print("启动失败")
        return
    end

    hwnd, found = window.find("记事本")
end

if found then
    -- 激活窗口
    window.activate(hwnd)
    util.sleep(200)

    -- 获取窗口信息
    local title = window.getTitle(hwnd)
    print("标题: " .. title)

    local bounds = window.getBounds(hwnd)
    print(string.format("位置: (%d, %d)", bounds.x, bounds.y))
    print(string.format("大小: %dx%d", bounds.width, bounds.height))
end
```

:::

---

## 可用接口

### `find(title)`

查找指定标题的窗口。

**参数：**
- `title` (string) - 窗口标题（支持部分匹配）

**返回：**
- `(number, boolean)` - 窗口句柄 (HWND)，是否找到

### `activate(hwnd)`

激活窗口（置顶）。

**参数：**
- `hwnd` (number) - 窗口句柄

**返回：**
- `boolean` - 是否成功

### `get_foreground()` / `getForeground()`

获取当前前台窗口。

**返回：**
- `number` - 前台窗口句柄 (HWND)

### `get_title(hwnd)` / `getTitle(hwnd)`

获取窗口标题。

**参数：**
- `hwnd` (number) - 窗口句柄

**返回：**
- `string` - 窗口标题

### `get_bounds(hwnd)` / `getBounds(hwnd)`

获取窗口边界。

**返回：**
- `dict/table` - 包含 `x`, `y`, `width`, `height`

### `wait_for(title, timeout)` / `waitFor(title, timeout)`

等待窗口出现。

**参数：**
- `title` (string) - 窗口标题（支持部分匹配）
- `timeout` (number) - 超时时间（毫秒），默认 5000

**返回：**
- `boolean` - 是否在超时前找到窗口
