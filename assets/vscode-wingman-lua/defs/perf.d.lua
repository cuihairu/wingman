--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— perf 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== perf 模块 ==========

---@class perf
local perf = {}

---@return any
function perf.clearCache() end

---@param path string
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param threshold number
---@return any
function perf.fastFindImage(path, x, y, w, h, threshold) end

---@return any
function perf.getCacheSize() end

---@return any
function perf.getCacheStats() end

---@return any
function perf.getConfig() end

---@return any
function perf.getStats() end

---@param color integer
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param tolerance integer
---@param maxCount integer
---@return any
function perf.parallelFindColors(color, x, y, w, h, tolerance, maxCount) end

---@param path string
function perf.preloadImage(path) end

---@return any
function perf.resetStats() end

---@param config any
function perf.setConfig(config) end

return perf
