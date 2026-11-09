--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— wingman 命名空间聚合面

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符生成；各模块的函数级
    声明按模块拆在 defs/<模块名>.d.lua（42 个导入集合），
    本文件只把全部模块组合到 wingman.* 命名空间。CI 的 api-typing-audit
    job 重生成后 git diff --exit-code 保持同步。

    使用方法：
    1. 将本文件与 defs/ 目录一起放在你的脚本项目根目录
    2. VSCode 安装 EmmyLua 插件
    3. 享受代码提示！（保留字函数如 bt.repeat 以 ["repeat"] 形式补全）
]=]

---@class wingman
---@field bt bt
---@field clipboard clipboard
---@field config config
---@field crypto crypto
---@field db db
---@field debugger debugger
---@field event event
---@field file file
---@field filewatcher filewatcher
---@field fsm fsm
---@field gameprofile gameprofile
---@field hotkey hotkey
---@field http http
---@field human human
---@field inbox inbox
---@field ini ini
---@field input input
---@field json json
---@field kv kv
---@field macro macro
---@field ml ml
---@field node node
---@field notify notify
---@field ocr ocr
---@field orchestration orchestration
---@field perf perf
---@field process process
---@field screen screen
---@field script script
---@field security security
---@field smarttrigger smarttrigger
---@field system system
---@field systemwatch systemwatch
---@field task task
---@field team team
---@field timer timer
---@field transport transport
---@field uia uia
---@field util util
---@field verification verification
---@field vision vision
---@field window window
local wingman = {}

return wingman
