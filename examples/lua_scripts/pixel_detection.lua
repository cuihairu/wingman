-- 像素颜色检测示例
-- 演示如何查找屏幕上的特定颜色

local wingman = require("wingman")

print("=== 像素颜色检测示例 ===")

-- 定义要查找的颜色 (红色 0xFF0000)
local targetColor = 0xFF0000
local tolerance = 10  -- 容差值

-- 获取屏幕尺寸
local screenW = wingman.screen.getScreenWidth()
local screenH = wingman.screen.getScreenHeight()

-- 定义搜索区域 (全屏)
local region = {
    x = 0,
    y = 0,
    width = screenW,
    height = screenH
}

print("正在查找颜色: 0x" .. string.format("%X", targetColor))
print("搜索区域: " .. region.x .. "," .. region.y .. "," .. region.width .. "," .. region.height)

-- 查找单个颜色点
local point, found = wingman.screen.findColor(targetColor, region, tolerance)

if found then
    print(string.format("找到颜色在位置: (%d, %d)", point.x, point.y))

    -- 在该位置点击
    wingman.input.click(point.x, point.y)
    print("已点击该位置")
else
    print("未找到指定颜色")
end

wingman.util.sleep(500)
print("脚本执行完成!")
