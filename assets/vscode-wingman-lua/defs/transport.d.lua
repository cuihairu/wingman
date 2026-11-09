--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— transport 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== transport 模块 ==========

---@class transport
local transport = {}

---@param handle integer
---@param data string
---@return boolean
function transport.tcpBroadcast(handle, data) end

---@param handle integer
---@param sessionId integer
---@return boolean
function transport.tcpCloseSession(handle, sessionId) end

---@param id string
---@param host string
---@param port integer
---@return any
function transport.tcpConnect(id, host, port) end

---@param handle integer
---@return boolean
function transport.tcpDisconnect(handle) end

---@param handle integer
---@return any
function transport.tcpGetSessions(handle) end

---@param handle integer
---@return boolean
function transport.tcpIsConnected(handle) end

---@param id string
---@param host string
---@param port integer
---@return any
function transport.tcpListen(id, host, port) end

---@param handle integer
---@param data string
---@return boolean
function transport.tcpSend(handle, data) end

---@param handle integer
---@param sessionId integer
---@param data string
---@return boolean
function transport.tcpSendTo(handle, sessionId, data) end

---@param handle integer
---@return boolean
function transport.tcpStop(handle) end

---@param handle integer
---@param host string
---@param port integer
---@return any
function transport.udpBind(handle, host, port) end

---@param handle integer
---@return boolean
function transport.udpClose(handle) end

---@param handle integer
---@param timeout integer
---@return any
function transport.udpRecvFrom(handle, timeout) end

---@param handle integer
---@param host string
---@param port integer
---@param data string
---@return boolean
function transport.udpSendTo(handle, host, port, data) end

---@param id string
---@return any
function transport.udpSocket(id) end

return transport
