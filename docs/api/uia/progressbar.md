# API: UIA ProgressBar

进度条（ProgressBar）控件显示操作进度，如加载进度、下载进度、安装进度等。

**重要**：进度条通常**只读**，无法通过脚本设置其值。

## 查找进度条

**说明**：进度条通常有描述性名称。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"下载进度"的进度条
progress = uia.find_by_name("下载进度")
if progress:
    print("找到进度条")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找名为"下载进度"的进度条
local progress = uia.findByName("下载进度")
if progress then
    print("找到进度条")
end
```

:::

---

## 获取进度值

**说明**：读取进度条的当前值，并计算完成百分比。

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
    if maximum > minimum:
        percent = (value - minimum) / (maximum - minimum) * 100
        print(f"进度: {percent:.1f}%")
    else:
        print(f"进度值: {value}")
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
    if maximum > minimum then
        local percent = (value - minimum) / (maximum - minimum) * 100
        print(string.format("进度: %.1f%%", percent))
    else
        print("进度值: " .. value)
    end
end
```

:::

---

## 等待进度完成

**说明**：轮询检查进度条，直到达到 100% 或超时。

**使用场景**：
- 等待文件下载完成
- 等待安装完成
- 等待加载完成

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def wait_for_progress_complete(progress_name, timeout=30000):
    """
    等待进度完成

    参数:
        progress_name: 进度条名称
        timeout: 超时时间（毫秒），默认 30 秒

    返回:
        True 表示完成，False 表示超时
    """
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

        util.sleep(500)  # 每 0.5 秒检查一次

    print("等待超时")
    return False

# 使用
if wait_for_progress_complete("安装进度", 60000):
    print("安装已完成")
else:
    print("安装未完成（超时）")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local function waitForProgressComplete(progressName, timeout)
    """
    等待进度完成

    参数:
        progressName: 进度条名称
        timeout: 超时时间（毫秒），默认 30 秒

    返回:
        true 表示完成，false 表示超时
    """
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

        util.sleep(500)  -- 每 0.5 秒检查一次
    end

    print("等待超时")
    return false
end

-- 使用
if waitForProgressComplete("安装进度", 60000) then
    print("安装已完成")
else
    print("安装未完成（超时）")
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取当前进度值 |
| `get_info()` | `:getInfo()` | 获取进度条信息 |

> **注意**：进度条通常只读，无法通过脚本设置其值。
