--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— security 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== security 模块 ==========

---@class security
local security = {}

---@param str string
---@return string
function security.filterSensitive(str) end

---@param length integer
---@return string
function security.generateRandomString(length) end

---@return any
function security.getRandomDelay() end

---@return any
function security.getRandomOffset() end

---@param str string
---@return string
function security.hashString(str) end

---@return any
function security.isDebuggerPresent() end

---@return any
function security.isRunningInVM() end

---@return any
function security.verifyIntegrity() end

return security
