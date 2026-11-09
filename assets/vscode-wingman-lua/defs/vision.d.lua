--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— vision 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== vision 模块 ==========

---@class vision
local vision = {}

---@param desc string
---@param region? any
---@return any
function vision.aiElements(desc, region) end

---@param desc string
---@param region? any
---@return any
function vision.aiLocate(desc, region) end

---@param cfg any
---@return boolean
function vision.aiSetup(cfg) end

---@param cfg any
---@return any
function vision.aiSetupLocal(cfg) end

---@return any
function vision.aiSetupStatus() end

---@param color any
---@param tolerance integer
---@param region? any
---@return any
function vision.findAllColors(color, tolerance, region) end

---@param color any
---@param tolerance integer
---@param region? any
---@return any
function vision.findColor(color, tolerance, region) end

---@param templatePath string
---@param threshold number
---@param region? any
---@return any
function vision.findImage(templatePath, threshold, region) end

---@param region? any
---@return any
function vision.getDominantColor(region) end

---@param color any
---@param tolerance integer
---@param region? any
---@return boolean
function vision.hasColor(color, tolerance, region) end

---@param name string
---@param path string
---@return boolean
function vision.templateAdd(name, path) end

---@param names table
---@param threshold number
---@param region? any
---@return any
function vision.templateFindAll(names, threshold, region) end

---@param name string
---@return boolean
function vision.templateHas(name) end

---@return any
function vision.templateList() end

---@param name string
---@return boolean
function vision.templateRemove(name) end

return vision
