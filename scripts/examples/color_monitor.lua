-- 颜色监控器
-- 监控屏幕上指定区域的颜色变化，当检测到目标颜色时执行操作

local wingman = require("wingman")

-- ==================== 配置 ====================

-- 监控区域 (屏幕坐标)
local MONITOR_REGION = {
    x = 100,
    y = 100,
    width = 50,
    height = 50
}

-- 目标颜色 (绿色表示技能冷却完成)
local TARGET_COLOR = {
    r = 0,
    g = 255,
    b = 0
}

-- 颜色容差 (0-255)
local COLOR_TOLERANCE = 30

-- 检测到颜色后执行的操作
local ACTION = {
    type = "key",      -- key, click, wait
    key = 0x31,        -- 1键 (虚拟键码)
    description = "按 1 键释放技能"
}

-- 检测间隔 (毫秒)
local CHECK_INTERVAL = 100

-- 冷却时间 (毫秒) - 防止重复触发
local COOLDOWN = 2000

-- ==================== 主程序 ====================

print("=== 颜色监控器 ===")
print(string.format("监控区域: (%d,%d) %dx%d", MONITOR_REGION.x, MONITOR_REGION.y, MONITOR_REGION.width, MONITOR_REGION.height))
print(string.format("目标颜色: RGB(%d,%d,%d)", TARGET_COLOR.r, TARGET_COLOR.g, TARGET_COLOR.b))
print(string.format("颜色容差: %d", COLOR_TOLERANCE))
print(string.format("操作: %s", ACTION.description))
print("按 Ctrl+C 停止监控")
print()

local lastTriggerTime = 0
local triggerCount = 0

-- 虚拟键码常量
local VK = {
    ["1"] = 0x31,
    ["2"] = 0x32,
    ["3"] = 0x33,
    ["4"] = 0x34,
    ["5"] = 0x35,
    F1 = 0x70,
    F2 = 0x71,
    F3 = 0x72,
    F4 = 0x73,
    F5 = 0x74,
    SPACE = 0x20,
    ENTER = 0x0D,
    ESC = 0x1B
}

-- 获取当前时间 (毫秒)
function getTime()
    return os.clock() * 1000
end

-- 检查是否在冷却中
function isInCooldown()
    return (getTime() - lastTriggerTime) < COOLDOWN
end

-- 执行操作
function executeAction()
    if isInCooldown() then
        return
    end

    triggerCount = triggerCount + 1
    lastTriggerTime = getTime()
    local timestamp = os.date("%H:%M:%S")

    if ACTION.type == "key" then
        print(string.format("[%s] 触发! #%d - 按键", timestamp, triggerCount))
        wingman.input.key(ACTION.key)
    elseif ACTION.type == "click" then
        print(string.format("[%s] 触发! #%d - 点击", timestamp, triggerCount))
        local centerX = MONITOR_REGION.x + MONITOR_REGION.width / 2
        local centerY = MONITOR_REGION.y + MONITOR_REGION.height / 2
        wingman.input.click(centerX, centerY)
    end
end

-- 主监控循环
while true do
    -- 检查目标颜色
    local result = wingman.screen.findColor(
        TARGET_COLOR,
        MONITOR_REGION.x,
        MONITOR_REGION.y,
        MONITOR_REGION.width,
        MONITOR_REGION.height,
        COLOR_TOLERANCE
    )

    if result then
        executeAction()
    end

    -- 等待下次检查
    wingman.input.delay(CHECK_INTERVAL)
end
