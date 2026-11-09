--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— smarttrigger 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== smarttrigger 模块 ==========

---@class smarttrigger
local smarttrigger = {}

---@param name string
---@param action any
---@return boolean
function smarttrigger.addAction(name, action) end

---@param name string
---@param condition any
---@return boolean
function smarttrigger.addCondition(name, condition) end

---@param name string
---@return boolean
function smarttrigger.create(name) end

---@param name string
---@return integer
function smarttrigger.getTriggerCount(name) end

---@param name string
---@return boolean
function smarttrigger.isRunning(name) end

---@param name string
function smarttrigger.remove(name) end

---@param name string
---@param ms integer
---@return boolean
function smarttrigger.setCheckInterval(name, ms) end

---@param name string
---@return boolean
function smarttrigger.start(name) end

---@param name string
---@return boolean
function smarttrigger.stop(name) end

return smarttrigger
