# API: wingman.gameprofile

游戏配置档案模块，用于管理游戏相关的配置信息。

## 模块概述

gameprofile 模块提供游戏档案的管理功能：
- **创建档案** - 创建新的游戏配置档案
- **加载档案** - 加载已有游戏档案
- **保存档案** - 保存游戏配置修改
- **列出档案** - 获取所有游戏档案列表
- **删除档案** - 删除指定游戏档案
- **当前游戏** - 设置和获取当前游戏

---

## 创建游戏档案

### create(id, profile) / create(id, profile)

**说明**：创建新的游戏档案。

**函数签名**：

```python
create(id: str, profile: dict) -> None
```

```lua
create(id: string, profile: table) -> nil
```

**参数**：
- `id` - 档案唯一标识
- `profile` - 档案数据，通常包含：
  - `name` / `name` - 游戏名称
  - `executable` / `executable` - 可执行文件路径
  - `window_title` / `window_title` - 游戏窗口标题

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import gameprofile

# 创建新的游戏档案
gameprofile.create("my_game", {
    "name": "我的游戏",
    "executable": "game.exe",
    "window_title": "游戏窗口"
})
```

== Lua

```lua:line-numbers
local gameprofile = require("wingman.gameprofile")

-- 创建新的游戏档案
gameprofile.create("my_game", {
    name = "我的游戏",
    executable = "game.exe",
    window_title = "游戏窗口"
})
```

:::

---

## 加载游戏档案

### load(id) / load(id)

**说明**：加载游戏档案。

**函数签名**：

```python
load(id: str) -> dict | None
```

```lua
load(id: string) -> table | nil
```

**参数**：
- `id` - 档案标识

**返回**：
- Python: 档案数据字典，不存在返回 `None`
- Lua: 档案数据表格，不存在返回 `nil`

:::tabs

== Python

```python:line-numbers
from wingman import gameprofile

# 加载游戏档案
profile = gameprofile.load("my_game")
if profile:
    print(f"游戏名称: {profile['name']}")
    print(f"可执行文件: {profile['executable']}")
```

== Lua

```lua:line-numbers
local gameprofile = require("wingman.gameprofile")

-- 加载游戏档案
local profile = gameprofile.load("my_game")
if profile then
    print("游戏名称: " .. profile.name)
    print("可执行文件: " .. profile.executable)
end
```

:::

---

## 保存游戏档案

### save(id, profile) / save(id, profile)

**说明**：保存游戏档案。

**函数签名**：

```python
save(id: str, profile: dict) -> None
```

```lua
save(id: string, profile: table) -> nil
```

**参数**：
- `id` - 档案标识
- `profile` - 档案数据

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import gameprofile

# 获取并修改档案
profile = gameprofile.load("my_game")
profile['resolution'] = {"width": 1920, "height": 1080}

# 保存修改
gameprofile.save("my_game", profile)
```

== Lua

```lua:line-numbers
local gameprofile = require("wingman.gameprofile")

-- 获取并修改档案
local profile = gameprofile.load("my_game")
profile.resolution = {width = 1920, height = 1080}

-- 保存修改
gameprofile.save("my_game", profile)
```

:::

---

## 列出所有档案

### list_all() / listAll()

**说明**：列出所有游戏档案。

**函数签名**：

```python
list_all() -> list[str]
```

```lua
listAll() -> table
```

**返回**：
- Python: 档案 ID 列表
- Lua: 档案 ID 数组

:::tabs

== Python

```python:line-numbers
from wingman import gameprofile

# 列出所有游戏档案
profiles = gameprofile.list_all()
for profile_id in profiles:
    profile = gameprofile.load(profile_id)
    print(f"- {profile['name']} ({profile_id})")
```

== Lua

```lua:line-numbers
local gameprofile = require("wingman.gameprofile")

-- 列出所有游戏档案
local profiles = gameprofile.listAll()
for i, profile_id in ipairs(profiles) do
    local profile = gameprofile.load(profile_id)
    print("- " .. profile.name .. " (" .. profile_id .. ")")
end
```

:::

---

## 删除档案

### delete(id) / delete(id)

**说明**：删除游戏档案。

**函数签名**：

```python
delete(id: str) -> None
```

```lua
delete(id: string) -> nil
```

**参数**：
- `id` - 档案标识

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import gameprofile

# 删除游戏档案
gameprofile.delete("my_game")
```

== Lua

```lua:line-numbers
local gameprofile = require("wingman.gameprofile")

-- 删除游戏档案
gameprofile.delete("my_game")
```

:::

---

## 设置当前游戏

### set_current(id) / setCurrent(id)

**说明**：设置当前游戏。

**函数签名**：

```python
set_current(id: str) -> None
```

```lua
setCurrent(id: string) -> nil
```

**参数**：
- `id` - 档案标识

**返回**：
- 无

---

## 获取当前游戏

### get_current() / getCurrent()

**说明**：获取当前游戏 ID。

**函数签名**：

```python
get_current() -> str
```

```lua
getCurrent() -> string
```

**返回**：
- 当前游戏 ID，未设置返回空字符串

:::tabs

== Python

```python:line-numbers
from wingman import gameprofile

# 设置当前游戏
gameprofile.set_current("my_game")

# 获取当前游戏
current = gameprofile.get_current()
print(f"当前游戏: {current}")
```

== Lua

```lua:line-numbers
local gameprofile = require("wingman.gameprofile")

-- 设置当前游戏
gameprofile.setCurrent("my_game")

-- 获取当前游戏
local current = gameprofile.getCurrent()
print("当前游戏: " .. current)
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `create(id, profile)` | `create(id, profile)` | 创建档案 | id: 档案ID<br>profile: 档案数据 |
| `load(id)` | `load(id)` | 加载档案 | id: 档案ID<br>返回: 档案数据或None/nil |
| `save(id, profile)` | `save(id, profile)` | 保存档案 | id: 档案ID<br>profile: 档案数据 |
| `list_all()` | `listAll()` | 列出所有档案 | 返回: 档案ID列表 |
| `delete(id)` | `delete(id)` | 删除档案 | id: 档案ID |
| `set_current(id)` | `setCurrent(id)` | 设置当前游戏 | id: 档案ID |
| `get_current()` | `getCurrent()` | 获取当前游戏 | 返回: 当前游戏ID或空字符串 |
