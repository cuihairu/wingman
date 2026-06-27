# API: wingman.macro

宏录制与回放模块，用于记录鼠标/键盘操作序列并按需回放。

## 模块概述

macro 模块提供录制、暂停、保存、加载与回放鼠标键盘操作序列的能力：

- **录制控制** - `start` / `stop` / `pause` / `resume` / `clear`
- **状态查询** - `isRecording` / `isPaused` / `getEventCount` / `status`
- **持久化** - `saveToLua` / `saveToJSON` / `loadFromJSON`
- **回放** - `playback`

录制的操作包括鼠标移动、点击、滚轮滚动，以及键盘按键与文本输入。

## 设计约束

1. **单例语义**：底层 `MacroRecorder` 通过全局 `g_instance` 指针接收平台 hook 回调，同一时刻实际只有一个录制器能生效。模块导出的函数始终操作同一个录制器实例（runtime 注入的实例优先；若未注入，则惰性创建模块自有的默认实例）。
2. **平台权限**：
   - **macOS**：依赖 CGEventTap，需在「系统设置 → 隐私与安全 → 辅助功能」中授予运行进程权限。
   - **Windows**：使用低层 `SetWindowsHookEx` 鼠标/键盘钩子，录制期间独占一个运行消息循环的线程。
   - **Linux**：依赖 X11 record 扩展。
3. **线程安全**：事件队列由互斥锁保护，hook 线程写入、其它线程读取/回放均安全。
4. **保存/加载**：`saveToLua` 导出为可执行的 Lua 脚本；`saveToJSON` 导出为结构化 JSON；`loadFromJSON` 仅支持 JSON 格式加载。

---

## 录制控制

### start()

**说明**：开始录制。返回调用后录制器是否处于录制中状态。

**函数签名**：

```python
start() -> bool
```

```lua
start() -> boolean
```

**参数**：无

**返回**：
- `bool`：调用后是否正在录制（`true` 表示录制已启动）

:::tabs

== Python

```python:line-numbers
from wingman import macro

recording = macro.start()
print("recording:", recording)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local recording = wingman.macro.start()
print("recording:", recording)
```

:::

---

### stop()

**说明**：停止录制。

**函数签名**：

```python
stop() -> bool
```

```lua
stop() -> boolean
```

**参数**：无

**返回**：
- `bool`：始终返回 `true`

:::tabs

== Python

```python:line-numbers
from wingman import macro

macro.stop()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.macro.stop()
```

:::

---

### pause()

**说明**：暂停录制（录制状态保持，但不再捕获新事件）。

**函数签名**：

```python
pause() -> bool
```

```lua
pause() -> boolean
```

**参数**：无

**返回**：
- `bool`：始终返回 `true`

:::tabs

== Python

```python:line-numbers
from wingman import macro

macro.pause()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.macro.pause()
```

:::

---

### resume()

**说明**：从暂停状态恢复录制。

**函数签名**：

```python
resume() -> bool
```

```lua
resume() -> boolean
```

**参数**：无

**返回**：
- `bool`：始终返回 `true`

:::tabs

== Python

```python:line-numbers
from wingman import macro

macro.resume()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.macro.resume()
```

:::

---

### clear()

**说明**：清空已录制的事件序列（不影响录制/暂停状态）。

**函数签名**：

```python
clear() -> bool
```

```lua
clear() -> boolean
```

**参数**：无

**返回**：
- `bool`：始终返回 `true`

:::tabs

== Python

```python:line-numbers
from wingman import macro

macro.clear()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.macro.clear()
```

:::

---

## 状态查询

### isRecording()

**说明**：查询当前是否正在录制。

**函数签名**：

```python
isRecording() -> bool
```

```lua
isRecording() -> boolean
```

**参数**：无

**返回**：
- `bool`：是否正在录制

:::tabs

== Python

```python:line-numbers
from wingman import macro

if macro.isRecording():
    print("正在录制")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.macro.isRecording() then
    print("正在录制")
end
```

:::

---

### isPaused()

**说明**：查询录制是否处于暂停状态。

**函数签名**：

```python
isPaused() -> bool
```

```lua
isPaused() -> boolean
```

**参数**：无

**返回**：
- `bool`：是否暂停

:::tabs

== Python

```python:line-numbers
from wingman import macro

if macro.isPaused():
    print("录制已暂停")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.macro.isPaused() then
    print("录制已暂停")
end
```

:::

---

### getEventCount()

**说明**：获取已录制的事件数量。

**函数签名**：

```python
getEventCount() -> int
```

```lua
getEventCount() -> number
```

**参数**：无

**返回**：
- `int`：已录制的事件数量

:::tabs

== Python

```python:line-numbers
from wingman import macro

count = macro.getEventCount()
print("events:", count)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local count = wingman.macro.getEventCount()
print("events:", count)
```

:::

---

### status()

**说明**：一次性获取录制的综合状态。

**函数签名**：

```python
status() -> dict
```

```lua
status() -> table
```

**参数**：无

**返回**：
- `dict` / `table`：状态对象，包含字段：
  - `recording` (bool)：是否正在录制
  - `paused` (bool)：是否暂停
  - `eventCount` (int)：已录制的事件数量

:::tabs

== Python

```python:line-numbers
from wingman import macro

s = macro.status()
print(s["recording"], s["paused"], s["eventCount"])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local s = wingman.macro.status()
print(s.recording, s.paused, s.eventCount)
```

:::

---

## 持久化

### saveToLua(path)

**说明**：将已录制的事件序列保存为可执行的 Lua 脚本。

**函数签名**：

```python
saveToLua(path: str) -> bool
```

```lua
saveToLua(path: string) -> boolean
```

**参数**：
- `path` (string)：目标文件路径

**返回**：
- `bool`：是否保存成功；参数缺失或非字符串时返回 `false`

:::tabs

== Python

```python:line-numbers
from wingman import macro

ok = macro.saveToLua("macro.lua")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.macro.saveToLua("macro.lua")
```

:::

---

### saveToJSON(path)

**说明**：将已录制的事件序列保存为结构化 JSON 文件。

**函数签名**：

```python
saveToJSON(path: str) -> bool
```

```lua
saveToJSON(path: string) -> boolean
```

**参数**：
- `path` (string)：目标文件路径

**返回**：
- `bool`：是否保存成功；参数缺失或非字符串时返回 `false`

:::tabs

== Python

```python:line-numbers
from wingman import macro

ok = macro.saveToJSON("macro.json")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.macro.saveToJSON("macro.json")
```

:::

---

### loadFromJSON(path)

**说明**：从 JSON 文件加载事件序列（覆盖当前已录制内容）。

**函数签名**：

```python
loadFromJSON(path: str) -> bool
```

```lua
loadFromJSON(path: string) -> boolean
```

**参数**：
- `path` (string)：JSON 文件路径

**返回**：
- `bool`：是否加载成功；参数缺失或非字符串时返回 `false`

:::tabs

== Python

```python:line-numbers
from wingman import macro

ok = macro.loadFromJSON("macro.json")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.macro.loadFromJSON("macro.json")
```

:::

---

## 回放

### playback(speed?, repeat?)

**说明**：回放已录制的事件序列。`speed` 控制回放速度（百分比），`repeat` 控制重复次数。

**函数签名**：

```python
playback(speed: int = 100, repeat: int = 1) -> bool
```

```lua
playback(speed: number = 100, repeat: number = 1) -> boolean
```

**参数**：
- `speed` (int, 可选)：回放速度百分比，默认 `100`（原速）；`200` 表示 2 倍速，`50` 表示半速
- `repeat` (int, 可选)：重复回放次数，默认 `1`

**返回**：
- `bool`：始终返回 `true`

:::tabs

== Python

```python:line-numbers
from wingman import macro

# 原速回放一次
macro.playback()

# 2 倍速回放 3 次
macro.playback(200, 3)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 原速回放一次
wingman.macro.playback()

-- 2 倍速回放 3 次
wingman.macro.playback(200, 3)
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `start()` | `start()` | 开始录制 | 无 |
| `stop()` | `stop()` | 停止录制 | 无 |
| `pause()` | `pause()` | 暂停录制 | 无 |
| `resume()` | `resume()` | 恢复录制 | 无 |
| `clear()` | `clear()` | 清空已录制事件 | 无 |
| `isRecording()` | `isRecording()` | 是否正在录制 | 无 |
| `isPaused()` | `isPaused()` | 是否暂停 | 无 |
| `getEventCount()` | `getEventCount()` | 已录制事件数 | 无 |
| `status()` | `status()` | 综合状态 | 无 |
| `saveToLua(path)` | `saveToLua(path)` | 保存为 Lua 脚本 | path: 文件路径 |
| `saveToJSON(path)` | `saveToJSON(path)` | 保存为 JSON | path: 文件路径 |
| `loadFromJSON(path)` | `loadFromJSON(path)` | 从 JSON 加载 | path: 文件路径 |
| `playback(speed?, repeat?)` | `playback(speed?, repeat?)` | 回放 | speed: 速度百分比(默认100)<br>repeat: 重复次数(默认1) |

## 完整示例

### 录制 + 回放（热键驱动）

:::tabs

== Python

```python:line-numbers
from wingman import macro, input, util

print("F6 开始录制 / F7 停止 / F8 回放")

while True:
    if input.isKeyPressed("VK_F6"):
        macro.start()
        print("recording started")
    elif input.isKeyPressed("VK_F7"):
        macro.stop()
        print("events:", macro.getEventCount())
    elif input.isKeyPressed("VK_F8"):
        macro.playback()

    util.sleep(50)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

print("F6 开始录制 / F7 停止 / F8 回放")

while true do
    if wingman.input.isKeyPressed("VK_F6") then
        wingman.macro.start()
        print("recording started")
    elseif wingman.input.isKeyPressed("VK_F7") then
        wingman.macro.stop()
        print("events:", wingman.macro.getEventCount())
    elseif wingman.input.isKeyPressed("VK_F8") then
        wingman.macro.playback()
    end

    wingman.util.sleep(50)
end
```

:::

### 录制 + 持久化 + 加载回放

:::tabs

== Python

```python:line-numbers
from wingman import macro

# 录制
macro.start()
# ... 用户在此期间操作 ...
macro.stop()
print("captured", macro.getEventCount(), "events")

# 保存
macro.saveToLua("session.lua")
macro.saveToJSON("session.json")

# 清空后重新加载
macro.clear()
macro.loadFromJSON("session.json")
print("loaded", macro.getEventCount(), "events")

# 以 1.5 倍速回放两次
macro.playback(150, 2)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 录制
wingman.macro.start()
-- ... 用户在此期间操作 ...
wingman.macro.stop()
print("captured", wingman.macro.getEventCount(), "events")

-- 保存
wingman.macro.saveToLua("session.lua")
wingman.macro.saveToJSON("session.json")

-- 清空后重新加载
wingman.macro.clear()
wingman.macro.loadFromJSON("session.json")
print("loaded", wingman.macro.getEventCount(), "events")

-- 以 1.5 倍速回放两次
wingman.macro.playback(150, 2)
```

:::

## 注意事项

1. **平台权限**：录制依赖系统级输入 hook，macOS 必须授予「辅助功能」权限，否则 hook 不会产生事件。
2. **单例语义**：同一时刻全局只有一个生效的录制器；多次 `start()` 不会创建多个并行录制流。
3. **回放阻塞**：`playback()` 在事件回放期间会按事件间延时阻塞执行，调用方需自行安排在非关键路径上调用。
4. **格式匹配**：`loadFromJSON` 仅识别 `saveToJSON` 产生的 JSON 结构；不能用于加载 `saveToLua` 输出的脚本。
5. **参数校验**：`saveToLua` / `saveToJSON` / `loadFromJSON` 在参数缺失或类型错误（非字符串）时返回 `false`，不抛异常。
