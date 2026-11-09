--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— inbox 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== inbox 模块 ==========

---@class inbox
local inbox = {}

---@param handle integer
---@param msgId string
---@return boolean
function inbox.ack(handle, msgId) end

---@param url string
---@param config? table
---@return any
function inbox.connect(url, config) end

---@param handle? integer
---@param timeout? integer
---@return any
function inbox.consume(handle, timeout) end

---@param handle? integer
---@return boolean
function inbox.disconnect(handle) end

---@param handle? integer
---@return boolean
function inbox.heartbeat(handle) end

---@param handle? integer
---@return boolean
function inbox.isConnected(handle) end

---@param handle integer
---@param msgId string
---@param result? table
---@return boolean
function inbox.report(handle, msgId, result) end

return inbox
