# API: wingman.gameprofile

游戏配置档案模块，用于管理游戏相关的配置信息。

## 创建游戏档案

::: code-group

```python [Python]
from wingman import gameprofile

# 创建新的游戏档案
gameprofile.create("my_game", {
    "name": "我的游戏",
    "executable": "game.exe",
    "window_title": "游戏窗口"
})
```

```lua [Lua]
local gameprofile = require("wingman.gameprofile")

-- 创建新的游戏档案
gameprofile.create("my_game", {
    name = "我的游戏",
    executable = "game.exe",
    window_title = "游戏窗口"
})
```

:::

## 加载游戏档案

::: code-group

```python [Python]
from wingman import gameprofile

# 加载游戏档案
profile = gameprofile.load("my_game")
if profile:
    print(f"游戏名称: {profile['name']}")
    print(f"可执行文件: {profile['executable']}")
```

```lua [Lua]
local gameprofile = require("wingman.gameprofile")

-- 加载游戏档案
local profile = gameprofile.load("my_game")
if profile then
    print("游戏名称: " .. profile.name)
    print("可执行文件: " .. profile.executable)
end
```

:::

## 保存游戏档案

::: code-group

```python [Python]
from wingman import gameprofile

# 获取并修改档案
profile = gameprofile.load("my_game")
profile['resolution'] = {"width": 1920, "height": 1080}

# 保存修改
gameprofile.save("my_game", profile)
```

```lua [Lua]
local gameprofile = require("wingman.gameprofile")

-- 获取并修改档案
local profile = gameprofile.load("my_game")
profile.resolution = {width = 1920, height = 1080}

-- 保存修改
gameprofile.save("my_game", profile)
```

:::

## 列出所有档案

::: code-group

```python [Python]
from wingman import gameprofile

# 列出所有游戏档案
profiles = gameprofile.list_all()
for profile_id in profiles:
    profile = gameprofile.load(profile_id)
    print(f"- {profile['name']} ({profile_id})")
```

```lua [Lua]
local gameprofile = require("wingman.gameprofile")

-- 列出所有游戏档案
local profiles = gameprofile.listAll()
for i, profile_id in ipairs(profiles) do
    local profile = gameprofile.load(profile_id)
    print("- " .. profile.name .. " (" .. profile_id .. ")")
end
```

:::

## 删除档案

::: code-group

```python [Python]
from wingman import gameprofile

# 删除游戏档案
gameprofile.delete("my_game")
```

```lua [Lua]
local gameprofile = require("wingman.gameprofile")

-- 删除游戏档案
gameprofile.delete("my_game")
```

:::

## 设置/获取当前游戏

::: code-group

```python [Python]
from wingman import gameprofile

# 设置当前游戏
gameprofile.set_current("my_game")

# 获取当前游戏
current = gameprofile.get_current()
print(f"当前游戏: {current}")
```

```lua [Lua]
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

### `create(id, profile)` / `create(id, profile)`

创建新的游戏档案。

**参数：**
- `id` - 档案唯一标识
- `profile` / `Profile` - 档案数据

### `load(id)` / `load(id)`

加载游戏档案。

**参数：**
- `id` - 档案标识

**返回：**
- `dict/table` - 档案数据，不存在返回 None/nil

### `save(id, profile)` / `save(id, profile)`

保存游戏档案。

**参数：**
- `id` - 档案标识
- `profile` / `Profile` - 档案数据

### `list_all()` / `listAll()`

列出所有游戏档案。

**返回：**
- `list/table` - 档案 ID 列表

### `delete(id)` / `delete(id)`

删除游戏档案。

**参数：**
- `id` - 档案标识

### `set_current(id)` / `setCurrent(id)`

设置当前游戏。

**参数：**
- `id` - 档案标识

### `get_current()` / `getCurrent()`

获取当前游戏。

**返回：**
- `string` - 当前游戏 ID，未设置返回空字符串
