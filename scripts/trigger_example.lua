-- Wingman 触发器示例
-- 演示如何使用触发器实现自动化

print("=== 触发器示例 ===")

-- 这个脚本需要在 C++ 中配合 TriggerManager 使用
-- 以下是在 Lua 中定义触发器配置的示例

-- 触发器配置示例
local triggers = {
    {
        name = "血量低自动喝药",
        condition = {
            type = "ColorFound",      -- 检测颜色出现
            value = "0xFF0000",        -- 红色
            region = { x = 100, y = 100, width = 50, height = 50 }, -- 血条区域
            tolerance = 10,
            interval = 500,           -- 每 500ms 检查一次
            enabled = true
        },
        actions = {
            { type = "KeyPress", value = "49", delay = 100 },  -- 按 1 键
            { type = "Delay", delay = 2000 }                    -- 等待 2 秒（技能 CD）
        },
        oneShot = false,           -- 不是一次性触发
        cooldown = 3000            -- 冷却 3 秒
    },

    {
        name = "检测敌人并攻击",
        condition = {
            type = "ImageFound",     -- 检测图像出现
            value = "enemy.png",      -- 敌人图标
            region = { x = 0, y = 0, width = 1920, height = 1080 },
            tolerance = 85,          -- 85% 相似度
            interval = 1000,
            enabled = true
        },
        actions = {
            { type = "Click", x = 0, y = 0, button = 0 },     -- 点击（位置会被替换）
            { type = "Delay", delay = 500 },
            { type = "KeyPress", value = "1" },                -- 使用技能 1
            { type = "KeyPress", value = "2" },                -- 使用技能 2
            { type = "KeyPress", value = "3" },                -- 使用技能 3
        },
        oneShot = false,
        cooldown = 1000
    },

    {
        name = "窗口激活时自动运行",
        condition = {
            type = "WindowOpened",
            value = "Notepad",          -- 记事本窗口
            enabled = true
        },
        actions = {
            { type = "Type", value = "Hello from Wingman!", delay = 50 }
        },
        oneShot = true,
        cooldown = 0
    }
}

print("配置了 " .. #triggers .. " 个触发器:")
for i, trigger in ipairs(triggers) do
    print(string.format("  %d. %s", i, trigger.name))
end

print("\n注意: 触发器需要在 C++ 层通过 TriggerManager 加载和启动")
