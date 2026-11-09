--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— ini 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== ini 模块 ==========

---@class ini
local ini = {}

---@param content string
---@return any
function ini.decode(content) end

---@param data table
---@param section string
---@param key? string
---@return boolean
function ini.delete(data, section, key) end

---@param data table
---@return string
function ini.encode(data) end

---@param data table
---@param section string
---@param key? string
---@return any
function ini.get(data, section, key) end

---@param data table
---@param section string
---@param key string
---@return boolean
function ini.has_key(data, section, key) end

---@param data table
---@param section string
---@return boolean
function ini.has_section(data, section) end

---@param data table
---@param section string
---@return any
function ini.keys(data, section) end

---@param base table
---@param override table
---@return table
function ini.merge(base, override) end

---@param data table
---@return any
function ini.sections(data) end

---@param data table
---@param section string
---@param key string
---@param value string
---@return any
function ini.set(data, section, key, value) end

return ini
