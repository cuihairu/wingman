--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— file 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== file 模块 ==========

---@class file
local file = {}

---@param path string
---@param content string
---@return boolean
function file.append(path, content) end

---@param src string
---@param dst string
---@return boolean
function file.copy(src, dst) end

---@param path string
---@return boolean
function file.exists(path) end

---@param path string
---@return boolean
function file.isDir(path) end

---@param path string
---@return boolean
function file.isFile(path) end

---@param path string
---@return any
function file.listDir(path) end

---@param path string
---@return boolean
function file.mkdir(path) end

---@param src string
---@param dst string
---@return boolean
function file.move(src, dst) end

---@param path string
---@return any
function file.read(path) end

---@param path string
---@return boolean
function file.remove(path) end

---@param path string
---@return boolean
function file.removeAll(path) end

---@param path string
---@return any
function file.size(path) end

---@param path string
---@param content string
---@return boolean
function file.write(path, content) end

return file
