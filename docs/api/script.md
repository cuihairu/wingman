# API: wingman.script

script 模块是脚本管理控制面，提供对已注册脚本的列举、查询、加载、运行、停止、重载，以及全局环境变量与配置的读写、热重载开关。它面向运行期编排，让一个脚本能够反过来操控其他脚本的生命周期与运行时参数。

## 设计约束

1. **运行时依赖**
   - script 模块依赖 runtime 注入的 `ScriptManager`。若 runtime 未注入（`mgr()` 为空），所有调用将返回安全的空值（空数组、空串、`false`）而非抛异常。

2. **状态模型**
   - 脚本状态机：`unloaded` / `loaded` / `starting` / `running` / `paused` / `stopping` / `completed` / `error`。
   - `list()` 返回的每个脚本信息对象包含 `name`、`path`、`state`、`language`，以及可选的 `lastError`（仅当存在错误时）。

3. **环境与配置作用域**
   - `setEnv` / `getEnv` 当前操作的是 **全局** 脚本环境（非 per-script），`name` 参数仅用于保持调用形态一致，不会按脚本隔离环境变量。
   - `setConfig` / `getConfig` 操作的是 runtime 全局配置键值。

4. **热重载**
   - `setHotReload(true)` 会同时启用全局自动重载标志并启动热重载监视；`setHotReload(false)` 会关闭。
   - 单个脚本的自动重载由 `load` 时传入的 `autoReload` 配置项控制。

5. **参数校验**
   - 必填参数缺失或类型不符时返回 `false` / `""` / 空数组，不抛异常。调用方应检查返回值。

## 函数列表

script 模块在 C++ 侧以 `mod.name = "script"` 注册，共 13 个函数：

| 函数 | 签名 |
|------|------|
| `list` | `() -> array` |
| `getState` | `(name) -> string` |
| `isRunning` | `(name) -> bool` |
| `has` | `(name) -> bool` |
| `reload` | `(name) -> bool` |
| `run` | `(name) -> bool` |
| `stop` | `(name) -> bool` |
| `setEnv` | `(name, key, value) -> bool` |
| `getEnv` | `(name, key) -> string` |
| `setConfig` | `(key, value) -> bool` |
| `getConfig` | `(key) -> string` |
| `setHotReload` | `(enabled) -> bool` |
| `load` | `(name, path, config?) -> bool` |

---

## 查询类函数

### script.list()

列举所有已注册脚本的信息。

**参数：**
- 无

**返回：**
- array: 脚本信息对象数组，每项为 `{name, path, state, language, lastError?}`；runtime 未就绪时返回空数组

**返回字段：**
| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 脚本名称 |
| `path` | string | 脚本文件路径 |
| `state` | string | 当前状态（`unloaded`/`loaded`/`starting`/`running`/`paused`/`stopping`/`completed`/`error`） |
| `language` | string | 脚本语言（如 `lua`、`python`） |
| `lastError` | string? | 最近一次错误信息（仅当存在错误时出现） |

**示例：**

```python
from wingman import script

for s in script.list():
    print(s["name"], s["state"], s["language"])
```

```lua
local wingman = require("wingman")

for _, s in ipairs(wingman.script.list()) do
    print(s.name, s.state, s.language)
end
```

### script.getState(name)

获取指定脚本的当前状态字符串。

**参数：**
- `name` (string): 脚本名称

**返回：**
- string: 状态字符串；脚本不存在或 runtime 未就绪时返回 `"unknown"`

**示例：**

```python
from wingman import script

state = script.getState("combat")
if state == "running":
    print("combat is running")
```

```lua
local wingman = require("wingman")

local state = wingman.script.getState("combat")
if state == "running" then
    print("combat is running")
end
```

### script.isRunning(name)

判断指定脚本是否处于 `running` 状态。

**参数：**
- `name` (string): 脚本名称

**返回：**
- bool: 脚本状态为 `running` 时返回 `true`，否则 `false`；runtime 未就绪或参数缺失时返回 `false`

**示例：**

```python
from wingman import script

if script.isRunning("loot"):
    print("loot script is active")
```

```lua
local wingman = require("wingman")

if wingman.script.isRunning("loot") then
    print("loot script is active")
end
```

### script.has(name)

判断指定脚本是否已注册。

**参数：**
- `name` (string): 脚本名称

**返回：**
- bool: 已注册返回 `true`，否则 `false`；runtime 未就绪或参数缺失时返回 `false`

**示例：**

```python
from wingman import script

if script.has("auto-heal"):
    print("auto-heal registered")
```

```lua
local wingman = require("wingman")

if wingman.script.has("auto-heal") then
    print("auto-heal registered")
end
```

---

## 生命周期控制

### script.run(name)

启动指定脚本。

**参数：**
- `name` (string): 脚本名称

**返回：**
- bool: 启动成功返回 `true`，否则 `false`；runtime 未就绪或参数缺失时返回 `false`

**示例：**

```python
from wingman import script

script.run("combat")   # 启动战斗脚本
```

```lua
local wingman = require("wingman")

wingman.script.run("combat")
```

### script.stop(name)

停止指定脚本。

**参数：**
- `name` (string): 脚本名称

**返回：**
- bool: 停止成功返回 `true`，否则 `false`；runtime 未就绪或参数缺失时返回 `false`

**示例：**

```python
from wingman import script

script.stop("combat")  # 停止战斗脚本
```

```lua
local wingman = require("wingman")

wingman.script.stop("combat")
```

### script.reload(name)

重新加载指定脚本。

**参数：**
- `name` (string): 脚本名称

**返回：**
- bool: 重载成功返回 `true`，否则 `false`；runtime 未就绪或参数缺失时返回 `false`

**示例：**

```python
from wingman import script

script.reload("combat")  # 编辑脚本文件后热重载
```

```lua
local wingman = require("wingman")

wingman.script.reload("combat")
```

### script.load(name, path, config?)

加载一个脚本到脚本管理器。

**参数：**
- `name` (string): 脚本名称（注册标识）
- `path` (string): 脚本文件路径
- `config` (object?, optional): 加载配置对象
  - `autoReload` (bool?, optional): 是否启用文件变更自动重载
  - `sandboxed` (bool?, optional): 是否以沙箱模式运行
  - `timeoutMs` (int?, optional): 单次执行超时（毫秒），默认 `30000`

**返回：**
- bool: 加载成功返回 `true`，否则 `false`；runtime 未就绪或参数不足时返回 `false`

**示例：**

```python
from wingman import script

# 基本加载
script.load("new-script", "scripts/new_script.py")

# 带配置加载
script.load("new-script", "scripts/new_script.py", {
    "autoReload": True,
    "sandboxed": False,
    "timeoutMs": 60000
})
```

```lua
local wingman = require("wingman")

-- 基本加载
wingman.script.load("new-script", "scripts/new_script.lua")

-- 带配置加载
wingman.script.load("new-script", "scripts/new_script.lua", {
    autoReload = true,
    sandboxed = false,
    timeoutMs = 60000
})
```

---

## 环境变量

> 注：`setEnv` / `getEnv` 当前操作全局脚本环境，`name` 参数仅用于调用形态兼容，不会按脚本隔离。

### script.setEnv(name, key, value)

设置全局脚本环境变量。

**参数：**
- `name` (string): 脚本名称（保留参数，当前不参与隔离）
- `key` (string): 环境变量名
- `value` (string): 环境变量值

**返回：**
- bool: 设置成功返回 `true`；runtime 未就绪或参数不足（少于 3 个）时返回 `false`

**示例：**

```python
from wingman import script

script.setEnv("combat", "TARGET_MODE", "aggressive")
```

```lua
local wingman = require("wingman")

wingman.script.setEnv("combat", "TARGET_MODE", "aggressive")
```

### script.getEnv(name, key)

读取全局脚本环境变量。

**参数：**
- `name` (string): 脚本名称（保留参数，当前不参与隔离）
- `key` (string): 环境变量名

**返回：**
- string: 环境变量值；不存在或 runtime 未就绪时返回 `""`

**示例：**

```python
from wingman import script

mode = script.getEnv("combat", "TARGET_MODE")
```

```lua
local wingman = require("wingman")

local mode = wingman.script.getEnv("combat", "TARGET_MODE")
```

---

## 配置管理

### script.setConfig(key, value)

设置 runtime 全局配置项。

**参数：**
- `key` (string): 配置键
- `value` (string): 配置值

**返回：**
- bool: 设置成功返回 `true`；runtime 未就绪或参数不足时返回 `false`

**示例：**

```python
from wingman import script

script.setConfig("max_scripts", "32")
```

```lua
local wingman = require("wingman")

wingman.script.setConfig("max_scripts", "32")
```

### script.getConfig(key)

读取 runtime 全局配置项。

**参数：**
- `key` (string): 配置键

**返回：**
- string: 配置值；不存在或 runtime 未就绪时返回 `""`

**示例：**

```python
from wingman import script

max_scripts = script.getConfig("max_scripts")
```

```lua
local wingman = require("wingman")

local max_scripts = wingman.script.getConfig("max_scripts")
```

---

## 热重载

### script.setHotReload(enabled)

全局开启或关闭热重载监视。

**参数：**
- `enabled` (bool): `true` 启用（同时设置全局自动重载标志并启动监视），`false` 关闭并停止监视

**返回：**
- bool: 操作成功返回 `true`；runtime 未就绪或参数缺失时返回 `false`

**示例：**

```python
from wingman import script

script.setHotReload(True)   # 开发期开启热重载
# ...编辑脚本文件后自动重载...
script.setHotReload(False)  # 发布期关闭
```

```lua
local wingman = require("wingman")

wingman.script.setHotReload(true)
wingman.script.setHotReload(false)
```

---

## 完整示例

### Python

```python
from wingman import script

# 1. 动态加载并运行一个脚本
script.load("patrol", "scripts/patrol.py", {
    "autoReload": True,
    "sandboxed": False,
    "timeoutMs": 30000
})

if script.has("patrol"):
    script.run("patrol")

# 2. 设置环境并检查状态
script.setEnv("patrol", "REGION", "north")
print("state:", script.getState("patrol"))
print("running:", script.isRunning("patrol"))

# 3. 列举全部脚本
for s in script.list():
    print(f"- {s['name']} [{s['state']}] ({s['language']})")

# 4. 热重载与停止
script.reload("patrol")
script.stop("patrol")

# 5. 读写全局配置
script.setConfig("log_level", "debug")
print("log_level:", script.getConfig("log_level"))
```

### Lua

```lua
local wingman = require("wingman")
local script = wingman.script

-- 1. 动态加载并运行一个脚本
script.load("patrol", "scripts/patrol.lua", {
    autoReload = true,
    sandboxed = false,
    timeoutMs = 30000
})

if script.has("patrol") then
    script.run("patrol")
end

-- 2. 设置环境并检查状态
script.setEnv("patrol", "REGION", "north")
print("state:", script.getState("patrol"))
print("running:", script.isRunning("patrol"))

-- 3. 列举全部脚本
for _, s in ipairs(script.list()) do
    print(string.format("- %s [%s] (%s)", s.name, s.state, s.language))
end

-- 4. 热重载与停止
script.reload("patrol")
script.stop("patrol")

-- 5. 读写全局配置
script.setConfig("log_level", "debug")
print("log_level:", script.getConfig("log_level"))
```

## 注意事项

1. **runtime 未就绪**
   - 当 runtime 未注入 `ScriptManager` 时，所有函数返回空值（空数组、`""`、`false`），不抛异常。调用方应据此判断可用性。

2. **环境作用域**
   - `setEnv` / `getEnv` 当前为全局环境，不按 `name` 隔离；未来如改为 per-script 隔离，调用形态可保持不变。

3. **热重载粒度**
   - `setHotReload` 是全局开关；单脚本是否自动重载还取决于 `load` 时设置的 `autoReload` 配置。

4. **状态查询容错**
   - `getState` 对未知脚本返回 `"unknown"`；`isRunning` / `has` 对未知脚本返回 `false`，便于条件判断。

5. **配置值类型**
   - `setConfig` / `getConfig` 的值均为字符串，如需存储数字/布尔需调用方自行转换。

---

**相关文档：** [API 概览](overview.md)
