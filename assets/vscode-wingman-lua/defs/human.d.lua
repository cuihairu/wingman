--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— human 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== human 模块 ==========

---@class human
local human = {}

---@return any
function human.getConfig() end

---@param vkCode integer
function human.keyboard_down(vkCode) end

---@param vkCode integer
function human.keyboard_press(vkCode) end

---@param text string
---@param randomCase boolean
function human.keyboard_type(text, randomCase) end

---@param vkCode integer
function human.keyboard_up(vkCode) end

---@param x integer
---@param y integer
function human.mouse_click(x, y) end

---@param x integer
---@param y integer
function human.mouse_doubleClick(x, y) end

---@param fromX integer
---@param fromY integer
---@param toX integer
---@param toY integer
function human.mouse_drag(fromX, fromY, toX, toY) end

---@param x integer
---@param y integer
---@param duration integer
function human.mouse_move(x, y, duration) end

---@param x integer
---@param y integer
function human.mouse_rightClick(x, y) end

---@param x integer
---@param y integer
---@param delta integer
function human.mouse_scroll(x, y, delta) end

---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param duration integer
function human.moveMouse(x1, y1, x2, y2, duration) end

---@param x integer
---@param y integer
---@param button string
function human.naturalClick(x, y, button) end

---@param text string
function human.naturalType(text) end

---@param min integer
---@param max integer
function human.randomDelay(min, max) end

---@param key string
---@param value any
function human.setConfig(key, value) end

---@param min integer
---@param max integer
function human.setDelayRange(min, max) end

---@param speed number
function human.setMoveSpeed(speed) end

---@param variance number
function human.setTypingVariance(variance) end

return human
