--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— fsm 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== fsm 模块 ==========

---@class fsm
local fsm = {}

---@param name string
---@param initial string
---@return any
function fsm.create(name, initial) end

---@param machineId string
---@return any
function fsm.current(machineId) end

---@param machineId string
---@param event string
---@param payload? table
---@return boolean
function fsm.dispatch(machineId, event, payload) end

---@param machineId string
---@return boolean
function fsm.reset(machineId) end

---@param machineId string
---@param stateName string
---@param onEnter? function
---@param onExit? function
---@return boolean
function fsm.state(machineId, stateName, onEnter, onExit) end

---@param machineId string
---@param from string
---@param to string
---@param on? string
---@param guard? function
---@param action? function
---@return boolean
function fsm.transition(machineId, from, to, on, guard, action) end

return fsm
