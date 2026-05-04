-- 自动循环示例
-- 演示如何创建自动循环脚本

print("=== 自动循环示例 ===")
print("按 Ctrl+C 停止脚本")

-- 定义要查找的颜色
local targetColor = 0x00FF00 -- 绿色
local tolerance = 10

-- 获取屏幕尺寸
local screenW = screen.getScreenWidth()
local screenH = screen.getScreenHeight()

local region = {
    x = 0,
    y = 0,
    width = screenW,
    height = screenH
}

local maxLoops = 100  -- 最大循环次数
local loopCount = 0
local clickDelay = 1000  -- 每次点击后等待 1 秒

while loopCount < maxLoops do
    loopCount = loopCount + 1
    print(string.format("循环: %d/%d", loopCount, maxLoops))

    -- 查找颜色
    local point, found = screen.findColor(targetColor, region, tolerance)

    if found then
        print(string.format("  找到目标: (%d, %d)", point.x, point.y))

        -- 平滑移动鼠标
        input.move(point.x, point.y, 200)
        util.sleep(50)

        -- 点击
        input.click(point.x, point.y)
        print("  已点击")

        -- 等待
        util.sleep(clickDelay)

        -- 添加随机延迟，模拟人类行为
        input.randomDelay(100, 300)
    else
        print("  未找到目标，等待...")
        util.sleep(500)
    end
end

print("循环完成!")
