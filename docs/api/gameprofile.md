> [中文](../zh/api/gameprofile.md) | **English**

# API: wingman.gameprofile

Game profile module for managing game-related configuration profiles: windows/colors/images/triggers/scripts, with support for templates, import/export, and version management.

The underlying implementation is `GameProfileManager` (a singleton); each profile carries a `version` field to facilitate version management.

## Module Overview

| Category | Functions |
|------|------|
| Read | `get`, `getActive`, `list`, `findByWindow` |
| Write | `load`, `save`, `setActive`, `delete` |
| Directory | `setProfilesDirectory`, `getProfilesDirectory`, `scan` |
| Template | `createTemplate` |
| Import/Export | `exportJson`, `importJson`, `exportPackage`, `importPackage` |

```lua
local gameprofile = require("wingman.gameprofile")
```

```python
from wingman import gameprofile
```

---

## Directory and Scanning

### setProfilesDirectory(dir)

Sets the profile scan directory.

```lua
gameprofile.setProfilesDirectory("./profiles")
```

### getProfilesDirectory()

Returns the current profile directory.

### scan()

Scans the profile directory and loads every `profile.json` in it.

```lua
gameprofile.scan()
```

---

## Template

### createTemplate(gameName) [id, name]

Creates a default template from the game name and registers it (the id is derived from the game name: lowercased + spaces turned into underscores). The template carries an initial `version = "1.0.0"`. Returns `nil` if an id with the same name already exists.

```lua
local id, name = gameprofile.createTemplate("示例游戏")
-- id => "示例游戏" 派生（如 "example_game" 视实现），name => "示例游戏"
```

---

## Read

### list() [{id}]

Returns the list of ids of all loaded profiles.

```lua
local ids = gameprofile.list()
for _, id in ipairs(ids) do
    print(id)
end
```

### get(id) [name, title?]

Returns the name and window title of the given profile; returns `nil` if it does not exist.

### getActive() [id, name?]

Returns the currently active profile.

### findByWindow(title) [id, name?]

Matches a profile by window title.

---

## Write

### load(id) [bool]

Loads the given profile from the directory.

### save(id) [bool]

Saves the given profile to file.

### setActive(id) [bool]

Sets the currently active profile.

### delete(id) [bool]

Deletes the given profile.

---

## Import / Export

### exportJson(id) [json]

Exports a profile as a JSON string (including all fields: `version`, `window`, `colors`, `images`, `triggers`, `scripts`, `settings`).

```lua
local json = gameprofile.exportJson(id)
-- 可写入文件或网络传输
```

### importJson(json, id?) [bool]

Imports a profile from a JSON string (validated automatically; optionally pass an id to override the original id).

```lua
gameprofile.importJson(jsonStr)        -- 使用 JSON 内的 id
gameprofile.importJson(jsonStr, "my")  -- 强制 id 为 "my"
```

### exportPackage(id, outputPath) [bool]

Exports a profile as a standalone file package (currently implemented as a JSON file; reserved for future ZIP packaging).

### importPackage(packagePath) [bool]

Imports a profile from a file package.

---

## Version Management

The `GameProfile.version` field is persisted along with the profile (templates default to `1.0.0`). On import/export the version travels with the JSON and can be incremented within the profile as it is upgraded; its semantics are left to the script's own convention (e.g. `major.minor.patch`).

---

## Example

For a complete example see [`examples/lua_scripts/game_profile.lua`](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts/game_profile.lua):

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
