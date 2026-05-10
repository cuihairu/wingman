-- 自动点击器示例
-- 检测指定颜色并自动点击

local wingman = require("wingman")

-- 配置
local CONFIG = {
    -- 目标颜色 (红色)
    targetColor = { r = 255, g = 0, b = 0 },
    -- 搜索区域
    searchRegion = { x = 0, y = 0, width = 800, height = 600 },
    -- 颜色容差
    tolerance = 20,
    -- 最大点击次数 (0 表示无限制)
    maxClicks = 5,
    -- 点击间隔 (毫秒)
    clickInterval = 1000,
    -- 鼠标移动动画时间
    moveDuration = 200
}

print("=== 自动点击器 ===")
print("目标颜色: RGB(" .. CONFIG.targetColor.r .. "," .. CONFIG.targetColor.g .. "," .. CONFIG.targetColor.b .. ")")
print("搜索区域: " .. CONFIG.searchRegion.width .. "x" .. CONFIG.searchRegion.height)
print("颜色容差: " .. CONFIG.tolerance)
print("最大点击次数: " .. (CONFIG.maxClicks > 0 and CONFIG.maxClicks or "无限制"))
print("按 Ctrl+C 停止脚本")
print()

local clickCount = 0
local running = true

-- Ctrl+C 处理
function onExit()
    print()
    print("=== 脚本停止 ===")
    print("总点击次数: " .. clickCount)
    os.exit(0)
end

-- 主循环
while running do
    -- 检查是否达到最大点击次数
    if CONFIG.maxClicks > 0 and clickCount >= CONFIG.maxClicks then
        print("达到最大点击次数，脚本结束")
        break
    end

    -- 查找目标颜色
    local result = wingman.screen.findColor(
        CONFIG.targetColor,
        CONFIG.searchRegion.x,
        CONFIG.searchRegion.y,
        CONFIG.searchRegion.width,
        CONFIG.searchRegion.height,
        CONFIG.tolerance
    )

    if result then
        -- 找到目标，执行点击
        clickCount = clickCount + 1
        local timestamp = os.date("%H:%M:%S")
        print("[" .. timestamp .. "] 找到目标! 点击 #" .. clickCount .. " at (" .. result.x .. "," .. result.y .. ")")

        -- 移动鼠标并点击
        wingman.input.move(result.x, result.y, CONFIG.moveDuration)
        wingman.input.delay(50)
        wingman.input.click(result.x, result.y, wingman.input.MouseButton.Left)

        -- 等待指定时间
        wingman.input.delay(CONFIG.clickInterval)
    else
        -- 未找到目标
        print("未找到目标，等待...")
        wingman.input.delay(500)
    end
end

print("脚本结束")
