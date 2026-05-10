-- Wingman 触发器配置示例
-- 定义游戏自动化触发器

triggers = {
    -- 技能冷却检测触发器
    {
        name = "技能冷却检测",
        enabled = true,
        condition = {
            type = "pixel",        -- 像素检测
            color = "0x00FF00",    -- 绿色 (技能就绪)
            region = { 100, 100, 50, 50 },  -- 检测区域
            tolerance = 10         -- 颜色容差
        },
        action = {
            type = "key",          -- 按键操作
            key = "1"              -- 按 1 键
        },
        cooldown = 500            -- 冷却时间 500ms
    },

    -- 血量监控触发器
    {
        name = "血量监控",
        enabled = true,
        condition = {
            type = "pixel",
            color = "0xFF0000",    -- 红色 (低血量)
            region = { 50, 50, 20, 20 },
            tolerance = 15
        },
        action = {
            type = "key",
            key = "2"              -- 按 2 键使用药水
        },
        cooldown = 2000           -- 冷却时间 2 秒
    },

    -- 蓝量监控触发器
    {
        name = "蓝量监控",
        enabled = false,          -- 默认禁用
        condition = {
            type = "pixel",
            color = "0x0000FF",    -- 蓝色 (低蓝量)
            region = { 50, 80, 20, 20 },
            tolerance = 15
        },
        action = {
            type = "key",
            key = "3"              -- 按 3 键使用蓝药
        },
        cooldown = 5000
    }
}

print("触发器配置加载完成: " .. #triggers .. " 个触发器")
