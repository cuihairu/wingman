--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— clipboard 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== clipboard 模块 ==========

---@class clipboard
local clipboard = {}

---@return any
function clipboard.clear() end

---@return any
function clipboard.getFiles() end

---@return any
function clipboard.getHTML() end

---@return any
function clipboard.getImage() end

---@return any
function clipboard.getText() end

---@return any
function clipboard.hasFiles() end

---@return any
function clipboard.hasHTML() end

---@return any
function clipboard.hasImage() end

---@return any
function clipboard.hasText() end

---@return any
function clipboard.isEmpty() end

---@param files any
---@return boolean
function clipboard.setFiles(files) end

---@param html string
---@return boolean
function clipboard.setHTML(html) end

---@param imageData string
---@param width integer
---@param height integer
---@return boolean
function clipboard.setImage(imageData, width, height) end

---@param text string
---@return boolean
function clipboard.setText(text) end

return clipboard
