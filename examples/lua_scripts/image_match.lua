-- Wingman 图像匹配示例
-- 演示如何使用 OpenCV 进行图像匹配

local wingman = require("wingman")

print("=== 图像匹配示例 ===")

-- 图像文件路径 (需要提前准备)
local imagePath = "scripts/images/button.png"

-- 检测区域
local region = {
    x = 0,
    y = 0,
    width = wingman.screen.getScreenWidth(),
    height = wingman.screen.getScreenHeight()
}

-- 匹配阈值 (0.0 - 1.0)
local threshold = 0.9

print(string.format("正在搜索图像: %s", imagePath))

-- 查找图像（返回数组：result[1]=点表或nil，result[2]=是否找到）
local result = wingman.screen.findImage(imagePath, region, threshold)

if result[2] then
    local pt = result[1]
    print(string.format("找到图像! 位置: (%d, %d)", pt.x, pt.y))

    -- 点击图像中心
    wingman.input.click(pt.x, pt.y)
    print("已点击")

    -- 随机延迟，模拟人工操作
    wingman.input.randomDelay(100, 300)
else
    print("未找到图像")
end

print("=== 完成 ===")
