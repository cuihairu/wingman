--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— macro 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== macro 模块 ==========

---@class macro
local macro = {}

---@return any
function macro.clear() end

---@return any
function macro.getEventCount() end

---@return any
function macro.isPaused() end

---@return any
function macro.isRecording() end

---@param path string
---@return boolean
function macro.loadFromJSON(path) end

---@return any
function macro.pause() end

---@param speed? integer
---@param repeat_? integer
---@return boolean
function macro.playback(speed, repeat_) end

---@return any
function macro.resume() end

---@param path string
---@return boolean
function macro.saveToJSON(path) end

---@param path string
---@return boolean
function macro.saveToLua(path) end

---@return any
function macro.start() end

---@return any
function macro.status() end

---@return any
function macro.stop() end

return macro
