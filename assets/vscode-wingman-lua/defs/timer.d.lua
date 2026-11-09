--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— timer 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== timer 模块 ==========

---@class timer
local timer = {}

---@param ms integer
---@param callback function
---@return any
function timer.after(ms, callback) end

---@param timerId integer
---@return boolean
function timer.cancel(timerId) end

---@return any
function timer.clearAll() end

---@param timerId integer
---@return boolean
function timer.clearInterval(timerId) end

---@param timerId integer
---@return boolean
function timer.clearTimeout(timerId) end

---@param timerId integer
---@return boolean
function timer.clearTimer(timerId) end

---@return any
function timer.count() end

---@param ms integer
---@param callback function
---@return any
function timer.every(ms, callback) end

---@param timerId integer
---@return boolean
function timer.exists(timerId) end

---@param ms integer
---@param callback function
---@return any
function timer.setInterval(ms, callback) end

---@param ms integer
---@param callback function
---@return any
function timer.setTimeout(ms, callback) end

---@param ms integer
function timer.sleep(ms) end

return timer
