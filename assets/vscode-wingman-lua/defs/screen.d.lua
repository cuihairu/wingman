--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— screen 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== screen 模块 ==========

---@class screen
local screen = {}

---@return any
function screen.capture() end

---@param region any
---@return boolean
function screen.captureRegion(region) end

---@param color any
---@param region any
---@param tolerance integer
---@return any
function screen.findColor(color, region, tolerance) end

---@param color any
---@param region any
---@param tolerance integer
---@param maxCount integer
---@return any
function screen.findColors(color, region, tolerance, maxCount) end

---@param imagePath string
---@param region any
---@param threshold number
---@return any
function screen.findImage(imagePath, region, threshold) end

---@param x integer
---@param y integer
---@return any
function screen.getPixel(x, y) end

---@return any
function screen.getScreenHeight() end

---@return any
function screen.getScreenWidth() end

return screen
