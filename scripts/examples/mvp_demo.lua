-- Wingman MVP 示例脚本
-- 演示基本的屏幕捕获和输入模拟功能

local wingman = require("wingman")

print("=== Wingman MVP 示例 ===")
print()

-- 1. 获取屏幕信息
print("1. 屏幕信息:")
local screenWidth = wingman.screen.getScreenWidth()
local screenHeight = wingman.screen.getScreenHeight()
print("   屏幕尺寸: " .. screenWidth .. "x" .. screenHeight)
print()

-- 2. 截取屏幕
print("2. 截取屏幕...")
local screenshot = wingman.screen.capture(0, 0, 800, 600)
if screenshot then
    print("   截图成功: " .. screenshot.width .. "x" .. screenshot.height)
else
    print("   截图失败")
end
print()

-- 3. 获取像素颜色
print("3. 获取像素颜色:")
local x = math.floor(screenWidth / 2)
local y = math.floor(screenHeight / 2)
local pixel = wingman.screen.getPixel(x, y)
if pixel then
    print("   屏幕中心 (" .. x .. "," .. y .. ") 颜色: RGB(" .. pixel.r .. "," .. pixel.g .. "," .. pixel.b .. ")")
end
print()

-- 4. 查找颜色
print("4. 查找白色像素:")
local white = { r = 255, g = 255, b = 255 }
local result = wingman.screen.findColor(white, 0, 0, 400, 300, 10)
if result then
    print("   找到白色像素: (" .. result.x .. "," .. result.y .. ")")
else
    print("   未找到白色像素")
end
print()

-- 5. 获取鼠标位置
print("5. 鼠标位置:")
local mousePos = wingman.input.getMousePosition()
print("   当前鼠标位置: (" .. mousePos.x .. "," .. mousePos.y .. ")")
print()

-- 6. 鼠标移动（带动画）
print("6. 鼠标移动演示:")
local targetX = 500
local targetY = 300
print("   移动鼠标到 (" .. targetX .. "," .. targetY .. ")")
wingman.input.move(targetX, targetY, 500)  -- 500ms 动画
wingman.input.delay(200)
print()

-- 7. 点击演示
print("7. 点击演示:")
print("   执行左键单击...")
wingman.input.click(targetX, targetY, wingman.input.MouseButton.Left)
wingman.input.delay(500)
print()

-- 8. 键盘演示
print("8. 键盘演示:")
print("   发送 'Hello' 文本...")
wingman.input.type("Hello")
wingman.input.delay(200)
print()

-- 9. 组合操作
print("9. 组合操作:")
print("   移动鼠标并点击...")
wingman.input.move(600, 350, 300)
wingman.input.delay(100)
wingman.input.click(600, 350)
wingman.input.delay(300)
wingman.input.key(0x1B)  -- ESC 键
print()

print("=== 示例完成 ===")
print("提示: 按 Ctrl+C 退出脚本")
