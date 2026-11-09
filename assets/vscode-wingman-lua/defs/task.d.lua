--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— task 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== task 模块 ==========

---@class task
local task = {}

---@param taskId string
---@return boolean
function task.cancel(taskId) end

---@param taskId string
---@return any
function task.error(taskId) end

---@param taskId string
---@return boolean
function task.pause(taskId) end

---@param taskId string
---@return any
function task.result(taskId) end

---@param taskId string
---@return boolean
function task.resume(taskId) end

---@param taskId string
---@param options? any
---@return boolean
function task.retry(taskId, options) end

---@param taskId string
---@return any
function task.status(taskId) end

---@param work function
---@param options? any
---@return any
function task.submit(work, options) end

---@param taskId string
---@param timeoutMs? integer
---@return boolean
function task.wait(taskId, timeoutMs) end

return task
