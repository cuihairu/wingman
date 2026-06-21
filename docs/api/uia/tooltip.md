# API: UIA ToolTip

工具提示（ToolTip）控件显示鼠标悬停时的帮助信息。工具提示是临时性控件，只有在鼠标悬停在某个元素上时才会出现。

## 查找工具提示

**说明**：工具提示通常通过控件类型查找，因为它没有固定的名称。

**注意**：工具提示只有在鼠标悬停时才会出现，查找前需要先触发显示。

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

        # 移动鼠标到按钮中心
        input.move_mouse(center_x, center_y)
        print("已移动鼠标到帮助按钮")

        # 等待工具提示出现
        util.sleep(1000)

        # 查找工具提示
        tooltip = uia.find_by_control_type("ToolTip")
        if tooltip:
            print("工具提示已出现")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 先移动鼠标到按钮上触发工具提示
local btn = wingman.uia.findButton("帮助")
if btn then
    local info = btn:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        local centerX = rect.left + rect.width / 2
        local centerY = rect.top + rect.height / 2

        -- 移动鼠标到按钮中心
        wingman.input.moveMouse(centerX, centerY)
        print("已移动鼠标到帮助按钮")

        -- 等待工具提示出现
        wingman.util.sleep(1000)

        -- 查找工具提示
        local tooltip = wingman.uia.findByControlType("ToolTip")
        if tooltip then
            print("工具提示已出现")
        end
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

tooltip = uia.find_by_control_type("ToolTip")
if tooltip:
    info = tooltip.get_info()
    text = info.get('name', '')
    print(f"工具提示内容: {text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tooltip = wingman.uia.findByControlType("ToolTip")
if tooltip then
    local info = tooltip:getInfo()
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
    info = btn.get_info()
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        center_x = rect['left'] + rect['width'] // 2
        center_y = rect['top'] + rect['height'] // 2
        input.move_mouse(center_x, center_y)

        # 轮询等待工具提示出现（最多 1 秒）
        for i in range(10):
            tooltip = uia.find_by_control_type("ToolTip")
            if tooltip:
                tip_info = tooltip.get_info()
                text = tip_info.get('name', '')
                print(f"工具提示: {text}")
                break
            util.sleep(100)
        else:
            print("工具提示未出现")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 移动鼠标触发工具提示
local btn = wingman.uia.findButton("帮助")
if btn then
    local info = btn:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        local centerX = rect.left + rect.width / 2
        local centerY = rect.top + rect.height / 2
        wingman.input.moveMouse(centerX, centerY)

        -- 轮询等待工具提示出现（最多 1 秒）
        for i = 1, 10 do
            local tooltip = wingman.uia.findByControlType("ToolTip")
            if tooltip then
                local tipInfo = tooltip:getInfo()
                local text = tipInfo.name or ""
                print("工具提示: " .. text)
                break
            end
            wingman.util.sleep(100)
        else
            print("工具提示未出现")
        end
    end
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_control_type("ToolTip")` | `findByControlType("ToolTip")` | 按控件类型查找 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:getInfo()` | 获取工具提示信息（包含 name 属性） |

> **注意**：工具提示是临时性控件，只有在鼠标悬停时才会出现。
