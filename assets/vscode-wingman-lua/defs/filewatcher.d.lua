--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— filewatcher 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== filewatcher 模块 ==========

---@class filewatcher
local filewatcher = {}

---@return any
function filewatcher.getWatchedPaths() end

---@param path string
---@return boolean
function filewatcher.isWatching(path) end

---@param path string
---@return boolean
function filewatcher.unwatch(path) end

---@return any
function filewatcher.unwatchAll() end

---@param path string
---@param callback function
---@param recursive? boolean
---@return any
function filewatcher.watch(path, callback, recursive) end

return filewatcher
