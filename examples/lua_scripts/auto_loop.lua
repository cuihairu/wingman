-- Wingman 自动循环示例
-- 演示如何使用触发器实现自动化循环

local wingman = require("wingman")

print("=== 自动循环示例 ===")

-- 配置触发器
local trigger = {
    name = "血量低自动喝药",
    condition = {
        type = "ColorFound",
        value = "0xFF0000",  -- 红色表示血量低
        region = {x = 100, y = 100, width = 50, height = 50},
        tolerance = 10,
        interval = 100  -- 每100ms检查一次
    },
    actions = {
        {type = "KeyPress", value = "49"},  -- 按 1 键
        {type = "Delay", value = 500}       -- 延迟 500ms
    },
    oneShot = false,   -- 不只触发一次
    cooldown = 2000    -- 冷却 2 秒
}

-- 向服务器注册触发器
local function registerTrigger(triggerConfig)
    local resp = wingman.http.post("http://localhost:8080/api/trigger/register",
                           wingman.json.encode(triggerConfig))
    if resp.success then
        local result = wingman.json.decode(resp.body)
        print(string.format("触发器已注册: ID=%s", result.triggerId))
        return result.triggerId
    else
        print("注册触发器失败: " .. resp.body)
        return nil
    end
end

-- 主循环
local function mainLoop()
    print("开始自动循环...")

    while true do
        -- 检查血量颜色
        local found = wingman.screen.findColor(0xFF0000, 100, 100, 50, 50, 10)

        if found then
            print("血量低! 使用药品...")
            wingman.input.key(49)  -- 按 1 键
            wingman.util.sleep(2000)  -- 等待 2 秒
        end

        wingman.util.sleep(100)  -- 每 100ms 检查一次
    end
end

-- 如果作为独立脚本运行
if arg and arg[0] and arg[0]:match("auto_loop%.lua") then
    mainLoop()
end

print("=== 完成 ===")
