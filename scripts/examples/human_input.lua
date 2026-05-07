-- 人性化输入示例脚本
-- 演示 human.mouse 和 human.keyboard 模块的使用

print("=== 人性化输入示例 ===")

-- 配置鼠标参数
human.mouse.setConfig({
    minMoveDuration = 100,
    maxMoveDuration = 300,
    moveVariance = 30,
    pathVariance = 15,
    clickDelayMin = 50,
    clickDelayMax = 150,
    enableRandomDelay = true,
    enablePathRandomness = true
})

-- 配置键盘参数
human.keyboard.setConfig({
    keyDownDelayMin = 30,
    keyDownDelayMax = 80,
    typeDelayMin = 80,
    typeDelayMax = 150,
    enableRandomDelay = true
})

-- 获取屏幕尺寸
local screenWidth = screen.getScreenWidth()
local screenHeight = screen.getScreenHeight()
print(string.format("屏幕尺寸: %dx%d", screenWidth, screenHeight))

-- 演示 1: 人性化鼠标移动
print("\n--- 演示 1: 人性化鼠标移动 ---")
print("鼠标将沿贝塞尔曲线移动到屏幕中心...")

local centerX = screenWidth / 2
local centerY = screenHeight / 2

human.mouse.move(centerX, centerY)
util.sleep(500)

-- 演示 2: 点击
print("\n--- 演示 2: 点击 ---")
print("在中心位置点击...")
human.mouse.click(centerX, centerY)
util.sleep(1000)

-- 演示 3: 拖拽
print("\n--- 演示 3: 拖拽 ---")
print("从中心拖拽到右下角...")
human.mouse.drag(centerX, centerY, centerX + 200, centerY + 200)
util.sleep(1000)

-- 演示 4: 滚动
print("\n--- 演示 4: 滚动 ---")
print("向上滚动...")
human.mouse.scroll(centerX, centerY, -120)
util.sleep(500)

print("向下滚动...")
human.mouse.scroll(centerX, centerY, 120)
util.sleep(1000)

-- 演示 5: 双击
print("\n--- 演示 5: 双击 ---")
print("在中心位置双击...")
human.mouse.doubleClick(centerX, centerY)
util.sleep(1000)

-- 演示 6: 右键点击
print("\n--- 演示 6: 右键点击 ---")
print("右键点击...")
human.mouse.rightClick(centerX, centerY)
util.sleep(1000)

-- 演示 7: 键盘输入
print("\n--- 演示 7: 键盘输入 ---")
print("按 F 键...")
human.keyboard.press(0x46)  -- F key
util.sleep(1000)

-- 演示 8: 文本输入（带随机延迟）
print("\n--- 演示 8: 文本输入 ---")
print("输入文本 'Hello, World!'...")
human.keyboard.type("Hello, World!")
util.sleep(1000)

-- 演示 9: 文本输入（带随机大小写）
print("\n--- 演示 9: 文本输入（随机大小写）---")
print("输入文本 'random case'...")
human.keyboard.type("random case", true)
util.sleep(1000)

-- 实际应用示例：自动化点击序列
print("\n--- 实际应用示例 ---")
print("执行自动化点击序列...")

local positions = {
    {x = centerX - 100, y = centerY - 100},
    {x = centerX + 100, y = centerY - 100},
    {x = centerX + 100, y = centerY + 100},
    {x = centerX - 100, y = centerY + 100}
}

for i, pos in ipairs(positions) do
    print(string.format("移动到位置 %d: (%d, %d)", i, pos.x, pos.y))
    human.mouse.click(pos.x, pos.y)
    util.sleep(500)
end

print("\n=== 示例完成 ===")
print("提示: 可以调整 human.mouse.setConfig() 和 human.keyboard.setConfig() 参数来改变行为")
