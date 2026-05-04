-- Wingman 脚本管理示例
-- 演示脚本管理器的高级功能

local wingman = require("wingman")

print("=== Wingman 脚本管理示例 ===\n")

-- 1. 加载脚本配置
print("1. 脚本配置")
local config = {
    autoReload = true,      -- 启用热加载
    sandboxed = true,       -- 启用沙箱模式
    timeout = 30000         -- 超时时间 (ms)
}

-- 2. 设置环境变量
print("2. 设置环境变量")
script.setEnv("main", "API_KEY", "your_api_key_here")
script.setEnv("main", "DEBUG", "true")
print("环境变量已设置\n")

-- 3. 设置配置
print("3. 设置脚本配置")
script.setConfig("main", "interval", "1000")
script.setConfig("main", "maxRetries", "3")
print("配置已保存\n")

-- 4. 启用热加载
print("4. 启用热加载")
script.setHotReload(true)
print("热加载已启用\n")

-- 5. 列出所有脚本
print("5. 脚本列表")
local scripts = script.list()
for name, info in pairs(scripts) do
    print(string.format("  - %s: %s", name, tostring(info)))
end
print()

-- 6. 脚本状态
print("6. 脚本状态查询")
local state = script.getState("main")
print(string.format("主脚本状态: %s", state))

local running = script.isRunning("main")
print(string.format("正在运行: %s", tostring(running)))
print()

-- 7. 动态加载子脚本
print("7. 动态加载子脚本")
--[[
    local subScriptLoaded = script.load("sub", "scripts/sub_script.lua", {
        autoReload = false,
        sandboxed = true,
        timeout = 5000
    })
    print("子脚本加载: " .. tostring(subScriptLoaded))
]]
print()

-- 8. 配置文件使用示例
print("8. 配置文件")
print("支持格式: .ini, .json")
print("示例配置文件内容:")
print([[
    [settings]
    interval=1000
    timeout=30000

    [game]
    process=Game.exe
    window=Game Window
]])
print()

-- 9. 热加载演示
print("9. 热加载演示")
print("编辑脚本文件后，系统会自动重新加载")
print("可以通过 script.checkReload() 手动检查\n")

-- 10. 沙箱安全
print("10. 沙箱安全")
print("沙箱模式下禁用的功能:")
print("  - io.* (文件 I/O)")
print("  - os.* (操作系统调用)")
print("  - debug.* (调试功能)")
print("  - package.* (模块加载)")
print()

print("=== 示例完成 ===")
