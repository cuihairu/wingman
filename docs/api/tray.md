# API: wingman.tray

系统托盘图标模块，用于在任务栏显示托盘图标和弹出菜单。

## 创建托盘图标

::: code-group

```python [Python]
from wingman import tray

icon = tray.create("Wingman")
```

```lua [Lua]
local tray = require("wingman.tray")

local icon = tray.create("Wingman")
```

:::

## 获取/移除托盘图标

::: code-group

```python [Python]
from wingman import tray

# 获取
icon = tray.get("main")

# 移除
tray.remove("main")
```

```lua [Lua]
local tray = require("wingman.tray")

-- 获取
local icon = tray.get("main")

-- 移除
tray.remove("main")
```

:::

## 设置图标

::: code-group

```python [Python]
from wingman import tray

icon = tray.create("Wingman")
icon.set_icon("C:/path/to/icon.ico")
```

```lua [Lua]
local tray = require("wingman.tray")

local icon = tray.create("Wingman")
icon:setIcon("C:/path/to/icon.ico")
```

:::

## 设置提示文本

::: code-group

```python [Python]
from wingman import tray

icon.set_tooltip("Wingman 自动化引擎")
```

```lua [Lua]
local tray = require("wingman.tray")

icon:setTooltip("Wingman 自动化引擎")
```

:::

## 添加菜单项

::: code-group

```python [Python]
from wingman import tray

icon.add_item("start", "启动脚本", lambda: print("启动脚本!"))
```

```lua [Lua]
local tray = require("wingman.tray")

icon:addItem("start", "启动脚本", function()
    print("启动脚本!")
end)
```

:::

## 添加分隔线

::: code-group

```python [Python]
from wingman import tray

icon.add_separator("sep1")
```

```lua [Lua]
local tray = require("wingman.tray")

icon:addSeparator("sep1")
```

:::

## 添加子菜单

::: code-group

```python [Python]
from wingman import tray

icon.add_submenu("scripts", "脚本", [
    {"id": "s1", "label": "脚本1"},
    {"id": "s2", "label": "脚本2"}
])
```

```lua [Lua]
local tray = require("wingman.tray")

icon:addSubmenu("scripts", "脚本", {
    {id = "s1", label = "脚本1"},
    {id = "s2", label = "脚本2"}
})
```

:::

## 移除/清空菜单项

::: code-group

```python [Python]
from wingman import tray

# 移除单项
icon.remove_item("start")

# 清空所有
icon.clear_items()
```

```lua [Lua]
local tray = require("wingman.tray")

-- 移除单项
icon:removeItem("start")

-- 清空所有
icon:clearItems()
```

:::

## 显示/隐藏图标

::: code-group

```python [Python]
from wingman import tray

# 显示
icon.show()

# 隐藏
icon.hide()
```

```lua [Lua]
local tray = require("wingman.tray")

-- 显示
icon:show()

-- 隐藏
icon:hide()
```

:::

## 检查可见性

::: code-group

```python [Python]
from wingman import tray

if icon.is_visible():
    print("图标可见")
```

```lua [Lua]
local tray = require("wingman.tray")

if icon:isVisible() then
    print("图标可见")
end
```

:::

## 销毁图标

::: code-group

```python [Python]
from wingman import tray

icon.destroy()
```

```lua [Lua]
local tray = require("wingman.tray")

icon:destroy()
```

:::

---

## 完整示例

::: code-group

```python [Python]
from wingman import tray, util

# 创建托盘图标
icon = tray.create("Wingman")

# 设置图标
icon.set_icon("assets/icon.ico")

# 添加菜单项
icon.add_item("start", "启动脚本", lambda: print("启动脚本..."))
icon.add_item("stop", "停止脚本", lambda: print("停止脚本..."))

# 添加分隔线
icon.add_separator("sep1")

# 添加子菜单
icon.add_submenu("tools", "工具", [
    {"id": "t1", "label": "录制宏"},
    {"id": "t2", "label": "查看日志"}
])

# 添加退出项
icon.add_item("exit", "退出", lambda: exit(0))

# 显示图标
icon.show()

# 保持运行
while True:
    util.sleep(1000)
```

```lua [Lua]
local tray = require("wingman.tray")
local util = require("wingman.util")

-- 创建托盘图标
local icon = tray.create("Wingman")

-- 设置图标
icon:setIcon("assets/icon.ico")

-- 添加菜单项
icon:addItem("start", "启动脚本", function()
    print("启动脚本...")
end)

icon:addItem("stop", "停止脚本", function()
    print("停止脚本...")
end)

-- 添加分隔线
icon:addSeparator("sep1")

-- 添加子菜单
icon:addSubmenu("tools", "工具", {
    {id = "t1", label = "录制宏"},
    {id = "t2", label = "查看日志"}
})

-- 添加退出项
icon:addItem("exit", "退出", function()
    os.exit()
end)

-- 显示图标
icon:show()

-- 保持运行
while true do
    util.sleep(1000)
end
```

:::

---

## 可用接口

### `create(tooltip)`

创建一个新的托盘图标。

**参数：**
- `tooltip` - 鼠标悬停时显示的提示文本

**返回：** TrayIcon 对象

### `get(id)`

获取已创建的托盘图标。

### `remove(id)`

移除托盘图标。

### TrayIcon 对象方法

#### `set_icon(icon_path)` / `setIcon(iconPath)`

设置托盘图标的图标文件（.ico 格式）。

#### `set_tooltip(tooltip)` / `setTooltip(tooltip)`

设置鼠标悬停提示文本。

#### `add_item(id, label, callback)` / `addItem(id, label, callback)`

添加菜单项。

#### `add_separator(id)` / `addSeparator(id)`

添加分隔线。

#### `add_submenu(id, label, items)` / `addSubmenu(id, label, items)`

添加子菜单。

#### `remove_item(id)` / `removeItem(id)`

移除菜单项。

#### `clear_items()` / `clearItems()`

清空所有菜单项。

#### `show()`

显示托盘图标。

#### `hide()`

隐藏托盘图标。

#### `is_visible()` / `isVisible()`

检查托盘图标是否可见。

#### `destroy()`

销毁托盘图标。
