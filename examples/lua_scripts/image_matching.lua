-- 图像匹配示例
-- 演示如何使用 OpenCV 进行图像识别

local wingman = require("wingman")

print("=== 图像匹配示例 ===")

-- 模板图像路径 (需要准备一个小的 PNG 图片)
local templatePath = "template.png"

-- 获取屏幕尺寸
local screenW = wingman.screen.getScreenWidth()
local screenH = wingman.screen.getScreenHeight()

-- 定义搜索区域
local region = {
    x = 0,
    y = 0,
    width = screenW,
    height = screenH
}

-- 匹配阈值 (0.0 - 1.0, 越高越严格)
local threshold = 0.85

print("正在查找图像: " .. templatePath)
print("匹配阈值: " .. threshold)

-- 查找图像
local point, found = wingman.screen.findImage(templatePath, region, threshold)

if found then
    print(string.format("找到图像在位置: (%d, %d)", point.x, point.y))

    -- 移动鼠标到该位置
    wingman.input.move(point.x, point.y, 500) -- 500ms 平滑移动
    wingman.util.sleep(100)

    -- 点击
    wingman.input.click(point.x, point.y)
    print("已点击该位置")
else
    print("未找到匹配的图像")
    print("提示: 请确保 template.png 文件存在")
end

print("脚本执行完成!")
