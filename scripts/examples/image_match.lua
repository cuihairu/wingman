-- Wingman 图像匹配示例
-- 演示如何使用 OpenCV 进行图像匹配

print("=== 图像匹配示例 ===")

-- 图像文件路径 (需要提前准备)
local imagePath = "scripts/images/button.png"

-- 检测区域
local region = {
    x = 0,
    y = 0,
    width = screen.getScreenWidth(),
    height = screen.getScreenHeight()
}

-- 匹配阈值 (0.0 - 1.0)
local threshold = 0.9

print(string.format("正在搜索图像: %s", imagePath))

-- 查找图像
local found, x, y = screen.findImage(imagePath, region.x, region.y,
                                     region.width, region.height, threshold)

if found then
    print(string.format("找到图像! 位置: (%d, %d)", x, y))

    -- 点击图像中心
    input.click(x, y)
    print("已点击")

    -- 随机延迟，模拟人工操作
    input.randomDelay(100, 300)
else
    print("未找到图像")
end

print("=== 完成 ===")
