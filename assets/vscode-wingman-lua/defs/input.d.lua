--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— input 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== input 模块 ==========

---@class input
local input = {}

---@param x integer
---@param y integer
---@param button integer
function input.click(x, y, button) end

---@param ms integer
function input.delay(ms) end

---@param vkCode integer
function input.key(vkCode) end

---@param vkCode integer
function input.keyDown(vkCode) end

---@param vkCode integer
function input.keyUp(vkCode) end

---@param x integer
---@param y integer
---@param duration integer
function input.move(x, y, duration) end

---@param min integer
---@param max integer
function input.randomDelay(min, max) end

---@param x integer
---@param y integer
---@param delta integer
function input.scroll(x, y, delta) end

---@param text string
---@param delay integer
function input.type(text, delay) end

return input
