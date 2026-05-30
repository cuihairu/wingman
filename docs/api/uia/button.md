# API: UIA Button

按钮控件，用于触发操作。

## 查找按钮

### 按名称查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("确定")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("确定")
if btn then
    btn:click()
end
```

:::

### 按 AutomationId 查找（推荐）

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_by_id("btnOK")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findById("btnOK")
if btn then
    btn:click()
end
```

:::

---

## 操作按钮

### 点击按钮

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("提交")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("提交")
if btn then
    btn:click()
end
```

:::

### 双击按钮

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("运行")
if btn:
    btn.double_click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("运行")
if btn then
    btn:doubleClick()
end
```

:::

---

## 检测按钮状态

### 检查是否启用

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("提交")
if btn:
    info = btn.get_info()
    if info.get('is_enabled', True):
        btn.click()
        print("已点击")
    else:
        print("按钮已禁用")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("提交")
if btn then
    local info = btn:getInfo()
    if info.isEnabled then
        btn:click()
        print("已点击")
    else
        print("按钮已禁用")
    end
end
```

:::

### 等待按钮可点击

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def wait_for_button_enabled(name, timeout=5000):
    """等待按钮启用"""
    start = util.time()
    while util.time() - start < timeout:
        btn = uia.find_button(name)
        if btn:
            info = btn.get_info()
            if info.get('is_enabled', True):
                return btn
        util.sleep(200)
    return None

btn = wait_for_button_enabled("确定")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local function waitForButtonEnabled(name, timeout)
    timeout = timeout or 5000
    local start = util.time()

    while util.time() - start < timeout do
        local btn = uia.findButton(name)
        if btn then
            local info = btn:getInfo()
            if info.isEnabled then
                return btn
            end
        end
        util.sleep(200)
    end
    return nil
end

local btn = waitForButtonEnabled("确定")
if btn then
    btn:click()
end
```

:::

---

## 获取按钮信息

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("确定")
if btn:
    info = btn.get_info()
    print(f"名称: {info.get('name', '')}")
    print(f"类型: {info.get('control_type', '')}")
    print(f"启用: {info.get('is_enabled', True)}")
    print(f"可见: {info.get('is_visible', True)}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("确定")
if btn then
    local info = btn:getInfo()
    print("名称: " .. (info.name or ""))
    print("类型: " .. (info.controlType or ""))
    print("启用: " .. tostring(info.isEnabled or true))
    print("可见: " .. tostring(info.is_visible or true))
end
```

:::

---

## 可用接口

### 查找按钮

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_button(name)` | `findButton(name)` | 按名称查找按钮 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 按钮操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `click()` | `:click()` | 点击按钮 |
| `double_click()` | `:doubleClick()` | 双击按钮 |
| `get_info()` | `:getInfo()` | 获取按钮信息 |
