# API: wingman.gameprofile

游戏配置档案模块，用于管理游戏相关的配置档案（profile）：窗口/颜色/图像/触发器/脚本，支持模板、导入导出与版本管理。

底层实现为 `GameProfileManager`（单例），档案含 `version` 字段便于版本管理。

## 模块概览

| 分类 | 函数 |
|------|------|
| 读取 | `get`、`getActive`、`list`、`findByWindow` |
| 写入 | `load`、`save`、`setActive`、`delete` |
| 目录 | `setProfilesDirectory`、`getProfilesDirectory`、`scan` |
| 模板 | `createTemplate` |
| 导入导出 | `exportJson`、`importJson`、`exportPackage`、`importPackage` |

```lua
local gameprofile = require("wingman.gameprofile")
```

```python
from wingman import gameprofile
```

---

## 目录与扫描

### setProfilesDirectory(dir)

设置档案扫描目录。

```lua
gameprofile.setProfilesDirectory("./profiles")
```

### getProfilesDirectory()

返回当前档案目录。

### scan()

扫描档案目录，加载其中所有 `profile.json`。

```lua
gameprofile.scan()
```

---

## 模板

### createTemplate(gameName) → id, name

基于游戏名创建默认模板并注册（id 由游戏名派生：小写 + 空格转下划线）。模板包含初始 `version = "1.0.0"`。已存在同名 id 时返回 `nil`。

```lua
local id, name = gameprofile.createTemplate("示例游戏")
-- id => "示例游戏" 派生（如 "example_game" 视实现），name => "示例游戏"
```

---

## 读取

### list() → {id}

返回所有已加载档案的 id 列表。

```lua
local ids = gameprofile.list()
for _, id in ipairs(ids) do
    print(id)
end
```

### get(id) → name, title?

返回指定档案的名称与窗口标题；不存在返回 `nil`。

### getActive() → id, name?

返回当前激活档案。

### findByWindow(title) → id, name?

按窗口标题匹配档案。

---

## 写入

### load(id) → bool

从目录载入指定档案。

### save(id) → bool

保存指定档案到文件。

### setActive(id) → bool

设置当前激活档案。

### delete(id) → bool

删除指定档案。

---

## 导入 / 导出

### exportJson(id) → json

导出档案为 JSON 字符串（含 `version`、`window`、`colors`、`images`、`triggers`、`scripts`、`settings` 全部字段）。

```lua
local json = gameprofile.exportJson(id)
-- 可写入文件或网络传输
```

### importJson(json, id?) → bool

从 JSON 字符串导入档案（自动校验；可选指定 id 覆盖原 id）。

```lua
gameprofile.importJson(jsonStr)        -- 使用 JSON 内的 id
gameprofile.importJson(jsonStr, "my")  -- 强制 id 为 "my"
```

### exportPackage(id, outputPath) → bool

导出档案为独立文件包（当前实现为 JSON 文件，预留 ZIP 打包扩展）。

### importPackage(packagePath) → bool

从文件包导入档案。

---

## 版本管理

`GameProfile.version` 字段随档案持久化（模板默认 `1.0.0`）。导入导出时版本随 JSON 流转，可在档案内随升级递增，由脚本自行约定语义（如 `major.minor.patch`）。

---

## 示例

完整示例见 [`examples/lua_scripts/game_profile.lua`](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts/game_profile.lua)：

```lua
gameprofile.setProfilesDirectory("./profiles")
gameprofile.scan()
local id = gameprofile.createTemplate("示例游戏")
for _, pid in ipairs(gameprofile.list()) do
    print(pid)
end
local json = gameprofile.exportJson(id)  -- 备份/迁移
gameprofile.importJson(json, "restored")
```
