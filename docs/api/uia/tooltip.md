# API: UIA ToolTip

工具提示（ToolTip）控件显示鼠标悬停时的帮助信息。工具提示是临时性控件，只有在鼠标悬停在某个元素上时才会出现。

## 查找工具提示

**说明**：UIARole 枚举**没有 ToolTip 专用角色值**，`find_all_by_control_type` 无法按类型过滤工具提示；只能按名称匹配查找（`find_text`，不做角色过滤）。

**注意**：工具提示只有在鼠标悬停时才会出现，查找前需要先触发显示。

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 先移动鼠标到按钮上触发工具提示
btn = uia.find_button("帮助")
if btn:
    info = btn["get_info"]()
    rect = info["bounds"]
    center_x = rect["x"] + rect["width"] // 2
    center_y = rect["y"] + rect["height"] // 2

    # 移动鼠标到按钮中心
    input.move(center_x, center_y)
    print("已移动鼠标到帮助按钮")

    # 等待工具提示出现
    util.sleep(1000)

    # 按提示文本查找（find_text 不做角色过滤）
    tooltip = uia.find_text("显示帮助内容")
    if tooltip:
        print("工具提示已出现")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 先移动鼠标到按钮上触发工具提示
local btn = wingman.uia.find_button("帮助")
if btn then
    local info = btn:get_info()
    local rect = info.bounds
    local centerX = rect.x + rect.width / 2
    local centerY = rect.y + rect.height / 2

    -- 移动鼠标到按钮中心
    wingman.input.move(centerX, centerY)
    print("已移动鼠标到帮助按钮")

    -- 等待工具提示出现
    wingman.util.sleep(1000)

    -- 按提示文本查找（find_text 不做角色过滤）
    local tooltip = wingman.uia.find_text("显示帮助内容")
    if tooltip then
        print("工具提示已出现")
    end
end
```

:::

---

## 获取工具提示文本

**说明**：读取工具提示显示的文本内容。

:::tabs

== Python

```python:line-numbers
from wingman import uia

tooltip = uia.find_text("显示帮助内容")
if tooltip:
    info = tooltip["get_info"]()
    text = info.get('name', '')
    print(f"工具提示内容: {text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tooltip = wingman.uia.find_text("显示帮助内容")
if tooltip then
    local info = tooltip:get_info()
    local text = info.name or ""
    print("工具提示内容: " .. text)
end
```

:::

---

## 等待工具提示出现

**说明**：工具提示可能需要短暂延迟才会出现，使用轮询方式等待。

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 移动鼠标触发工具提示
btn = uia.find_button("帮助")
if btn:
    info = btn["get_info"]()
    rect = info["bounds"]
    center_x = rect["x"] + rect["width"] // 2
    center_y = rect["y"] + rect["height"] // 2
    input.move(center_x, center_y)

    # 轮询等待工具提示出现（最多 1 秒）
    for i in range(10):
        tooltip = uia.find_text("显示帮助内容")
        if tooltip:
            tip_info = tooltip["get_info"]()
            print(f"工具提示: {tip_info.get('name', '')}")
            break
        util.sleep(100)
    else:
        print("工具提示未出现")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 移动鼠标触发工具提示
local btn = wingman.uia.find_button("帮助")
if btn then
    local info = btn:get_info()
    local rect = info.bounds
    local centerX = rect.x + rect.width / 2
    local centerY = rect.y + rect.height / 2
    wingman.input.move(centerX, centerY)

    -- 轮询等待工具提示出现（最多 1 秒）
    local found = false
    for i = 1, 10 do
        local tooltip = wingman.uia.find_text("显示帮助内容")
        if tooltip then
            local tipInfo = tooltip:get_info()
            print("工具提示: " .. (tipInfo.name or ""))
            found = true
            break
        end
        wingman.util.sleep(100)
    end
    if not found then
        print("工具提示未出现")
    end
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_text(name)` | `find_text(name)` | 按名称查找（ToolTip 无专用 UIARole，不做角色过滤） |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:get_info()` | 获取工具提示信息（包含 name 属性） |

> **注意**：工具提示是临时性控件，只有在鼠标悬停时才会出现。
