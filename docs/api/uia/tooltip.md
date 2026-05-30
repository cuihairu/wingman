# API: UIA ToolTip

工具提示控件，显示鼠标悬停时的帮助信息。

## 查找工具提示

工具提示通常在鼠标悬停时才出现，需要先触发显示：

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 先移动鼠标到按钮上触发工具提示
btn = uia.find_button("帮助")
if btn:
    info = btn.get_info()
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        center_x = rect['left'] + rect['width'] // 2
        center_y = rect['top'] + rect['height'] // 2
        input.move_mouse(center_x, center_y)

        # 等待工具提示出现
        util.sleep(1000)

        # 查找工具提示
        tooltip = uia.find_by_control_type("ToolTip")
        if tooltip:
            print("工具提示已出现")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 先移动鼠标到按钮上触发工具提示
local btn = uia.findButton("帮助")
if btn then
    local info = btn:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        local centerX = rect.left + rect.width / 2
        local centerY = rect.top + rect.height / 2
        input.moveMouse(centerX, centerY)

        -- 等待工具提示出现
        util.sleep(1000)

        -- 查找工具提示
        local tooltip = uia.findByControlType("ToolTip")
        if tooltip then
            print("工具提示已出现")
        end
    end
end
```

:::

---

## 获取工具提示文本

:::tabs

== Python

```python:line-numbers
from wingman import uia

tooltip = uia.find_by_control_type("ToolTip")
if tooltip:
    info = tooltip.get_info()
    text = info.get('name', '')
    print(f"工具提示内容: {text}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tooltip = uia.findByControlType("ToolTip")
if tooltip then
    local info = tooltip:getInfo()
    local text = info.name or ""
    print("工具提示内容: " .. text)
end
```

:::

---

## 等待工具提示出现

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 移动鼠标触发工具提示
btn = uia.find_button("帮助")
if btn:
    info = btn.get_info()
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        center_x = rect['left'] + rect['width'] // 2
        center_y = rect['top'] + rect['height'] // 2
        input.move_mouse(center_x, center_y)

        # 轮询等待工具提示出现
        for i in range(10):  # 最多等待 1 秒
            tooltip = uia.find_by_control_type("ToolTip")
            if tooltip:
                tip_info = tooltip.get_info()
                print(f"工具提示: {tip_info.get('name', '')}")
                break
            util.sleep(100)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 移动鼠标触发工具提示
local btn = uia.findButton("帮助")
if btn then
    local info = btn:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        local centerX = rect.left + rect.width / 2
        local centerY = rect.top + rect.height / 2
        input.moveMouse(centerX, centerY)

        -- 轮询等待工具提示出现
        for i = 1, 10 do  -- 最多等待 1 秒
            local tooltip = uia.findByControlType("ToolTip")
            if tooltip then
                local tipInfo = tooltip:getInfo()
                print("工具提示: " .. (tipInfo.name or ""))
                break
            end
            util.sleep(100)
        end
    end
end
```

:::

---

## 可用接口

### 查找工具提示

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_control_type("ToolTip")` | `findByControlType("ToolTip")` | 按控件类型查找 |
| `find_by_name(name)` | `findByName(name)` | 按名称查找（如果已知） |

### 工具提示操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:getInfo()` | 获取工具提示信息（包含 name 属性） |

> **注意**：工具提示是临时性控件，只有在鼠标悬停时才会出现。
