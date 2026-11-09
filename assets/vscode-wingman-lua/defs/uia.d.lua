--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— uia 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== uia 模块 ==========

---@class uia
local uia = {}

---@param controlType integer
---@return any
function uia.find_all_by_control_type(controlType) end

---@param name string
---@return any
function uia.find_button(name) end

---@param id string
---@return any
function uia.find_by_id(id) end

---@param name string
---@return any
function uia.find_by_name(name) end

---@param name string
---@return any
function uia.find_check_box(name) end

---@param name string
---@return any
function uia.find_combo_box(name) end

---@param name string
---@return any
function uia.find_edit(name) end

---@param name string
---@return any
function uia.find_hyperlink(name) end

---@param name string
---@return any
function uia.find_image(name) end

---@param name string
---@return any
function uia.find_list(name) end

---@param name string
---@return any
function uia.find_list_item(name) end

---@param name string
---@return any
function uia.find_menu_item(name) end

---@param name string
---@return any
function uia.find_progress_bar(name) end

---@param name string
---@return any
function uia.find_radio_button(name) end

---@param name string
---@return any
function uia.find_slider(name) end

---@param name string
---@return any
function uia.find_spinner(name) end

---@param name string
---@return any
function uia.find_tab(name) end

---@param name string
---@return any
function uia.find_tab_item(name) end

---@param name string
---@return any
function uia.find_text(name) end

---@param name string
---@return any
function uia.find_tree(name) end

---@param name string
---@return any
function uia.find_tree_item(name) end

---@return any
function uia.from_foreground() end

---@param x integer
---@param y integer
---@return any
function uia.from_point(x, y) end

---@param hwnd integer
---@return any
function uia.from_window(hwnd) end

---@param name string
---@param callback function
---@return any
function uia.on_property_changed(name, callback) end

---@param name string
---@param callback function
---@return any
function uia.on_structure_changed(name, callback) end

---@param listenerId integer
---@return boolean
function uia.remove_event_listener(listenerId) end

---@param root? any
---@param maxDepth? integer
---@return any
function uia.tree(root, maxDepth) end

---@param selector any
---@param timeout integer
---@return any
function uia.wait_for(selector, timeout) end

---@param id string
---@param timeout integer
---@return any
function uia.wait_for_id(id, timeout) end

---@param name string
---@param timeout integer
---@return any
function uia.wait_for_name(name, timeout) end

---@param controlType integer
---@param timeout integer
---@return any
function uia.wait_for_role(controlType, timeout) end

return uia
