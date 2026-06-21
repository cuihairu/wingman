-- Wingman 脚本管理示例
-- 演示 wingman.script 模块（包装 runtime 的 ScriptManager）

local wingman = require("wingman")

print("=== Wingman 脚本管理示例 ===\n")

-- 1. 设置环境变量（注：当前为全局环境，name 参数保留以兼容调用形态）
print("1. 设置环境变量")
wingman.script.setEnv("main", "API_KEY", "your_api_key_here")
wingman.script.setEnv("main", "DEBUG", "true")
print("环境变量已设置\n")

-- 2. 设置配置
print("2. 设置脚本配置")
wingman.script.setConfig("interval", "1000")
wingman.script.setConfig("maxRetries", "3")
print("配置已保存\n")

-- 3. 启用热加载
print("3. 启用热加载")
wingman.script.setHotReload(true)
print("热加载已启用\n")

-- 4. 列出所有脚本
print("4. 脚本列表")
local scripts = wingman.script.list()
for i, info in ipairs(scripts) do
    print(string.format("  [%d] %s (%s)", i, info.name, info.state))
end
print()

-- 5. 脚本状态
print("5. 脚本状态查询")
local state = wingman.script.getState("main")
print(string.format("主脚本状态: %s", state))

local running = wingman.script.isRunning("main")
print(string.format("正在运行: %s", tostring(running)))
print()

-- 6. 动态加载子脚本
print("6. 动态加载子脚本")
-- wingman.script.load("sub", "scripts/sub_script.lua", {
--     autoReload = false, sandboxed = false, timeoutMs = 5000
-- })
print()

print("=== 示例完成 ===")
print("提示: wingman.script 操作的是 runtime 的 ScriptManager")
print("      在 GUI/agent 模式下可管理多个脚本；单脚本 CLI 模式下 list() 返回空")
