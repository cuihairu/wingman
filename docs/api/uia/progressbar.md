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
local wingman = require("wingman")

-- 查找名为"下载进度"的进度条
local progress = wingman.uia.find_by_name("下载进度")
if progress then
    print("找到进度条")
end
```

:::

---

## 获取进度值

> **未实现（计划中）**：`get_info()` 的返回键中**没有数值进度字段**（无 `value`/`minimum`/`maximum`）；`get_value()` 返回的是元素文本（实现为 `getText`），不是数值进度。若应用恰好把进度写进元素文本/名称（如 "80%"），可经 `get_value()` 读取后自行解析，但这不是保证存在的形态。

:::tabs

== Python

```python:line-numbers
from wingman import uia

progress = uia.find_by_name("安装进度")
if progress:
    # get_info 无数值进度字段，这里只读取通用信息
    info = progress["get_info"]()
    print(f"名称: {info.get('name', '')}")

    # get_value 返回元素文本（部分应用会写入进度文本）
    text = progress["get_value"]()
    print(f"元素文本: {text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local progress = wingman.uia.find_by_name("安装进度")
if progress then
    -- get_info 无数值进度字段，这里只读取通用信息
    local info = progress:get_info()
    print("名称: " .. (info.name or ""))

    -- get_value 返回元素文本（部分应用会写入进度文本）
    local text = progress:get_value()
    print("元素文本: " .. text)
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
    start_time = util.get_time()

    while util.get_time() - start_time < timeout:
        progress = uia.find_by_name(progress_name)
        if progress:
            # get_info 无数值进度字段；若应用把进度写进元素文本（如 "100%"），
            # 可经 get_value() 读取并自行解析
            text = progress["get_value"]() or ""
            if "100" in text:
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
local wingman = require("wingman")

-- 等待进度完成：timeout 毫秒内轮询元素文本，完成返回 true，超时返回 false
local function waitForProgressComplete(progressName, timeout)
    timeout = timeout or 30000
    local startTime = wingman.util.getTime()

    while wingman.util.getTime() - startTime < timeout do
        local progress = wingman.uia.find_by_name(progressName)
        if progress then
            -- get_info 无数值进度字段；若应用把进度写进元素文本（如 "100%"），
            -- 可经 get_value() 读取并自行解析
            local text = progress:get_value() or ""
            if string.find(text, "100", 1, true) then
                print("进度完成！")
                return true
            end
        end

        wingman.util.sleep(500)  -- 每 0.5 秒检查一次
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
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:get_value()` | 获取元素文本（非数值进度，见上） |
| `get_info()` | `:get_info()` | 获取进度条通用信息 |

> **注意**：进度条通常只读，无法通过脚本设置其值。
