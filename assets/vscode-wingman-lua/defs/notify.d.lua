--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— notify 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== notify 模块 ==========

---@class notify
local notify = {}

---@param eventName string
---@param target string
---@param options? table
function notify.bridge(eventName, target, options) end

---@param message string
---@param meta? table
function notify.debug(message, meta) end

---@param message string
---@param meta? table
function notify.error(message, meta) end

---@param message string
---@param meta? table
function notify.info(message, meta) end

---@param title string
---@param message string
---@param level? string
function notify.toast(title, message, level) end

---@return any
function notify.trayHide() end

---@param text? string
---@return boolean
function notify.traySetBadge(text) end

---@param text string
---@return boolean
function notify.traySetTooltip(text) end

---@return any
function notify.trayShow() end

---@param message string
---@param meta? table
function notify.warn(message, meta) end

---@param url string
---@param payload? table
---@param options? table
function notify.webhook(url, payload, options) end

return notify
