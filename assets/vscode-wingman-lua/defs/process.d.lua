--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— process 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== process 模块 ==========

---@class process
local process = {}

---@param pid integer
---@return boolean
function process.exists(pid) end

---@param name string
---@return any
function process.find(name) end

---@param path string
---@param args string
---@param workdir string
---@return any
function process.start(path, args, workdir) end

---@param pid integer
---@param force boolean
---@return boolean
function process.terminate(pid, force) end

---@param pid integer
---@param timeout integer
---@return boolean
function process.wait(pid, timeout) end

---@param name string
---@param timeout integer
---@return boolean
function process.waitFor(name, timeout) end

return process
