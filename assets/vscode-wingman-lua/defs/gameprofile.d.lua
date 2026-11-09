--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— gameprofile 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== gameprofile 模块 ==========

---@class gameprofile
local gameprofile = {}

---@param gameName string
---@return any
function gameprofile.createTemplate(gameName) end

---@param id string
---@return boolean
function gameprofile.delete(id) end

---@param id string
---@return any
function gameprofile.exportJson(id) end

---@param id string
---@param outputPath string
---@return boolean
function gameprofile.exportPackage(id, outputPath) end

---@param title string
---@return any
function gameprofile.findByWindow(title) end

---@param id string
---@return any
function gameprofile.get(id) end

---@return any
function gameprofile.getActive() end

---@return any
function gameprofile.getProfilesDirectory() end

---@param json string
---@param id? string
---@return boolean
function gameprofile.importJson(json, id) end

---@param packagePath string
---@return boolean
function gameprofile.importPackage(packagePath) end

---@return any
function gameprofile.list() end

---@param id string
---@return boolean
function gameprofile.load(id) end

---@param id string
---@return boolean
function gameprofile.save(id) end

---@return any
function gameprofile.scan() end

---@param id string
---@return boolean
function gameprofile.setActive(id) end

---@param dir string
---@return boolean
function gameprofile.setProfilesDirectory(dir) end

return gameprofile
