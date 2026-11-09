--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— bt 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== bt 模块 ==========

---@class bt
local bt = {}

---@param name string
---@param callback function
---@return any
function bt.action(name, callback) end

---@param parentHandle integer
---@param childHandle integer
---@return boolean
function bt.addChild(parentHandle, childHandle) end

---@param name string
---@param callback function
---@return any
function bt.condition(name, callback) end

---@param name string
---@return boolean
function bt.create(name) end

---@param childHandle integer
---@return any
function bt.inverter(childHandle) end

---@param name string
---@param policy string
---@return any
function bt.parallel(name, policy) end

---@param name string
function bt.remove(name) end

---@param childHandle integer
---@param count integer
---@return any
bt["repeat"] = function(childHandle, count) end

---@param name string
---@return any
function bt.selector(name) end

---@param name string
---@return any
function bt.sequence(name) end

---@param treeName string
---@param nodeHandle integer
---@return boolean
function bt.setRoot(treeName, nodeHandle) end

---@param name string
---@return string
function bt.tick(name) end

---@param milliseconds integer
---@return any
function bt.wait(milliseconds) end

return bt
