--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— verification 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== verification 模块 ==========

---@class verification
local verification = {}

---@param period integer
---@return integer
function verification.remaining(period) end

---@param secret string
---@return string
function verification.steamGuard(secret) end

---@param secret string
---@param digits integer
---@param period integer
---@return string
function verification.totp(secret, digits, period) end

---@param secret string
---@param code string
---@param digits integer
---@param period integer
---@param window integer
---@return boolean
function verification.verify(secret, code, digits, period, window) end

return verification
