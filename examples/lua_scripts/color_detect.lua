-- Wingman 颜色检测示例
-- 演示如何检测屏幕上的特定颜色

local http = require("http")

print("=== 颜色检测示例 ===")

-- 定义要查找的颜色 (红色: 0xFF0000)
local targetColor = 0xFF0000

-- 检测区域 (全屏)
local region = {
    x = 0,
    y = 0,
    width = screen.getScreenWidth(),
    height = screen.getScreenHeight()
}

-- 容差值
local tolerance = 10

print("正在搜索红色...")

-- 查找单个颜色点
local found, x, y = screen.findColor(targetColor, region.x, region.y,
                                      region.width, region.height, tolerance)

if found then
    print(string.format("找到红色! 位置: (%d, %d)", x, y))

    -- 在该位置点击
    input.click(x, y)
    print("已点击")
else
    print("未找到红色")
end

-- 查找所有红色点
print("\n正在搜索所有红色点...")
local points = screen.findColors(targetColor, region.x, region.y,
                                   region.width, region.height, tolerance, 10)

print(string.format("找到 %d 个红色点", #points))
for i, point in ipairs(points) do
    print(string.format("  [%d] (%d, %d)", i, point.x, point.y))
end

print("=== 完成 ===")
