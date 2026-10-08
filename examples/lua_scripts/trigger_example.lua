-- Wingman 触发器示例
-- 演示 wingman.smarttrigger 的创建、条件/动作配置、启停与状态查询

local wingman = require("wingman")

print("=== 触发器示例 ===")

-- 1. 创建触发器
local names = { "血量低自动喝药", "检测敌人并攻击", "提示文本出现自动确认" }
for _, name in ipairs(names) do
    if wingman.smarttrigger.create(name) then
        print("已创建触发器: " .. name)
    end
end

-- 2. 配置条件与动作

-- 血量低自动喝药：血条区域出现红色 → 按 1 键 + 等待 2 秒（技能 CD）
wingman.smarttrigger.addCondition("血量低自动喝药", {
    type = "COLOR_FOUND",
    color = 0xFF0000,        -- 红色
    tolerance = 10,
    region = { x = 100, y = 100, width = 50, height = 50 }  -- 血条区域
})
wingman.smarttrigger.addAction("血量低自动喝药", { type = "KEY_PRESS", key = 49 })  -- 按 1 键
wingman.smarttrigger.addAction("血量低自动喝药", { type = "WAIT", waitMs = 2000 })
wingman.smarttrigger.setCheckInterval("血量低自动喝药", 500)

-- 检测敌人并攻击：全屏出现敌人图标 → 点击 + 依次按技能键
wingman.smarttrigger.addCondition("检测敌人并攻击", {
    type = "IMAGE_FOUND",
    template = "enemy.png",  -- 敌人图标
    threshold = 0.85,        -- 85% 相似度
    region = { x = 0, y = 0, width = 1920, height = 1080 }
})
wingman.smarttrigger.addAction("检测敌人并攻击", { type = "CLICK", x = 0, y = 0 })
wingman.smarttrigger.addAction("检测敌人并攻击", { type = "WAIT", waitMs = 500 })
wingman.smarttrigger.addAction("检测敌人并攻击", { type = "KEY_PRESS", key = 49 })  -- 技能 1
wingman.smarttrigger.addAction("检测敌人并攻击", { type = "KEY_PRESS", key = 50 })  -- 技能 2
wingman.smarttrigger.addAction("检测敌人并攻击", { type = "KEY_PRESS", key = 51 })  -- 技能 3
wingman.smarttrigger.setCheckInterval("检测敌人并攻击", 1000)

-- 提示文本出现自动确认：OCR 命中"确认" → 日志提示
wingman.smarttrigger.addCondition("提示文本出现自动确认", {
    type = "OCR_CONTAINS",
    text = "确认",
    region = { x = 0, y = 0, width = 800, height = 200 }
})
wingman.smarttrigger.addAction("提示文本出现自动确认", { type = "LOG", message = "检测到确认提示！" })
wingman.smarttrigger.setCheckInterval("提示文本出现自动确认", 1000)

print("\n配置了 " .. #names .. " 个触发器:")
for i, name in ipairs(names) do
    print(string.format("  %d. %s", i, name))
end

-- 3. 启动触发器
print("\n启动触发器...")
for _, name in ipairs(names) do
    if wingman.smarttrigger.start(name) then
        print("已启动: " .. name)
    else
        print("启动失败: " .. name)
    end
end

-- 4. 查询运行状态
print("\n运行状态:")
for _, name in ipairs(names) do
    print(string.format("  %s 运行中: %s, 条件数: %d",
        name,
        tostring(wingman.smarttrigger.isRunning(name)),
        wingman.smarttrigger.getTriggerCount(name)))
end

-- 5. 演示窗口期（触发器在后台按检查间隔轮询条件）
print("\n触发器运行中（5 秒演示窗口）...")
wingman.util.sleep(5000)

-- 6. 停止并移除触发器
for _, name in ipairs(names) do
    wingman.smarttrigger.stop(name)
    wingman.smarttrigger.remove(name)
end

print("\n=== 触发器示例完成 ===")
