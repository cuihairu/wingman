> [中文](tooltip.md) | **English**

# API: UIA ToolTip

The ToolTip control displays help information when the mouse hovers. ToolTips are temporary controls that only appear when the mouse hovers over an element.

## Finding a ToolTip

**Note**: The UIARole enum **has no ToolTip-specific role value**; `find_all_by_control_type` cannot filter tooltips by type. You can only find them by name match (`find_text`, no role filtering).

**Note**: ToolTips only appear when the mouse hovers; trigger the tooltip to show before searching.

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

## Getting the tooltip text

**Note**: Reads the text content displayed by the tooltip.

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

## Waiting for the tooltip to appear

**Note**: A tooltip may take a brief delay to appear; wait for it by polling.

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

## Available Interfaces

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `find_text(name)` | `find_text(name)` | Find by name (ToolTip has no dedicated UIARole; no role filtering) |

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `get_info()` | `:get_info()` | Get tooltip information (includes the name attribute) |

> **Note**: ToolTips are temporary controls that only appear when the mouse hovers.
