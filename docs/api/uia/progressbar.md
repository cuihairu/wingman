# API: UIA ProgressBar

进度条控件，显示操作进度。进度条通常只读，不能直接设置值。

## 查找进度条

:::tabs

== Python

```python:line-numbers
from wingman import uia

progress = uia.find_by_name("下载进度")
if progress:
    print("找到进度条")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local progress = uia.findByName("下载进度")
if progress then
    print("找到进度条")
end
```

:::

---

## 获取进度值

:::tabs

== Python

```python:line-numbers
from wingman import uia

progress = uia.find_by_name("安装进度")
if progress:
    info = progress.get_info()
    value = info.get('value', 0)
    minimum = info.get('minimum', 0)
    maximum = info.get('maximum', 100)

    # 计算百分比
    percent = (value - minimum) / (maximum - minimum) * 100
    print(f"进度: {percent:.1f}%")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local progress = uia.findByName("安装进度")
if progress then
    local info = progress:getInfo()
    local value = info.value or 0
    local minimum = info.minimum or 0
    local maximum = info.maximum or 100

    -- 计算百分比
    local percent = (value - minimum) / (maximum - minimum) * 100
    print(string.format("进度: %.1f%%", percent))
end
```

:::

---

## 等待进度完成

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def wait_for_progress_complete(progress_name, timeout=30000):
    """等待进度完成"""
    start_time = util.time()

    while util.time() - start_time < timeout:
        progress = uia.find_by_name(progress_name)
        if progress:
            info = progress.get_info()
            value = info.get('value', 0)
            maximum = info.get('maximum', 100)

            if value >= maximum:
                print("进度完成！")
                return True

        util.sleep(500)

    print("等待超时")
    return False

# 使用
wait_for_progress_complete("安装进度")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local function waitForProgressComplete(progressName, timeout)
    timeout = timeout or 30000
    local startTime = util.time()

    while util.time() - startTime < timeout do
        local progress = uia.findByName(progressName)
        if progress then
            local info = progress:getInfo()
            local value = info.value or 0
            local maximum = info.maximum or 100

            if value >= maximum then
                print("进度完成！")
                return true
            end
        end

        util.sleep(500)
    end

    print("等待超时")
    return false
end

-- 使用
waitForProgressComplete("安装进度")
```

:::

---

## 可用接口

### 查找进度条

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 进度条操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取当前进度值 |
| `get_info()` | `:getInfo()` | 获取进度条信息（包含 value/minimum/maximum） |

> **注意**：进度条通常只读，无法通过脚本设置其值。
