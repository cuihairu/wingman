--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— event 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== event 模块 ==========

---@class event
local event = {}

---@param type? string
function event.clear(type) end

---@param type string
---@param payload? table
---@param meta? any
---@return boolean
function event.emit(type, payload, meta) end

---@param id any
---@return any
function event.listener(id) end

---@param type string
---@return any
function event.listeners(type) end

---@param type string
---@param payload? table
---@param meta? table
---@return any
function event.message(type, payload, meta) end

---@param subscription any
---@return boolean
function event.off(subscription) end

---@param type string
---@param callback function
---@param name? string
---@return any
function event.on(type, callback, name) end

---@param type string
---@param callback function
---@return any
function event.once(type, callback) end

return event
