> [中文](progressbar.md) | **English**

# API: UIA ProgressBar

The ProgressBar control displays the progress of an operation, such as loading progress, download progress, or installation progress.

**Important**: ProgressBars are usually **read-only**; their value cannot be set via script.

## Finding a ProgressBar

**Note**: ProgressBars usually have descriptive names.

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

## Getting the progress value

> **Not implemented (planned)**: The `get_info()` return keys have **no numeric progress field** (no `value`/`minimum`/`maximum`); `get_value()` returns the element text (implemented as `getText`), not a numeric progress. If the application happens to write the progress into the element text/name (e.g. "80%"), you can read it via `get_value()` and parse it yourself, but this is not a guaranteed form.

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
    local info = progress.get_info()
    print("名称: " .. (info.name or ""))

    -- get_value 返回元素文本（部分应用会写入进度文本）
    local text = progress.get_value()
    print("元素文本: " .. text)
end
```

:::

---

## Waiting for progress to complete

**Note**: Poll the progress bar until it reaches 100% or times out.

**Use cases**:
- Wait for a file download to finish
- Wait for an installation to finish
- Wait for loading to finish

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
            local text = progress.get_value() or ""
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

## Available Interfaces

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `find_by_name(name)` | `find_by_name(name)` | Find by name |

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `get_value()` | `:get_value()` | Get the element text (not a numeric progress; see above) |
| `get_info()` | `:get_info()` | Get general information about the progress bar |

> **Note**: ProgressBars are usually read-only; their value cannot be set via script.
