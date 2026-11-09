--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— script 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== script 模块 ==========

---@class script
local script = {}

---@param key string
---@return string
function script.getConfig(key) end

---@param name string
---@param key string
---@return string
function script.getEnv(name, key) end

---@param name string
---@return string
function script.getState(name) end

---@param name string
---@return any
function script.getStats(name) end

---@param name string
---@return boolean
function script.has(name) end

---@param name string
---@return boolean
function script.isRunning(name) end

---@return any
function script.list() end

---@param name string
---@param path string
---@param config? any
---@return boolean
function script.load(name, path, config) end

---@param name string
---@return boolean
function script.reload(name) end

---@param name string
---@return boolean
function script.run(name) end

---@param key string
---@param value string
---@return boolean
function script.setConfig(key, value) end

---@param name string
---@param key string
---@param value string
---@return boolean
function script.setEnv(name, key, value) end

---@param enabled boolean
---@return boolean
function script.setHotReload(enabled) end

---@param name string
---@return boolean
function script.stop(name) end

return script
