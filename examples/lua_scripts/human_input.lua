-- 人性化输入示例脚本
-- 演示 wingman.human 模块的使用（mouse_* / keyboard_* 系列函数）

local wingman = require("wingman")

print("=== 人性化输入示例 ===")

-- 配置拟人化参数（高层字段：delay_min / delay_max / move_speed / typing_variance）
wingman.human.setConfig("delay_min", 50)
wingman.human.setConfig("delay_max", 150)
wingman.human.setConfig("move_speed", 1.0)
wingman.human.setConfig("typing_variance", 0.5)
print("拟人化参数已设置\n")

-- 获取屏幕尺寸
local screenWidth = wingman.screen.getScreenWidth()
local screenHeight = wingman.screen.getScreenHeight()
print(string.format("屏幕尺寸: %dx%d", screenWidth, screenHeight))

-- 演示 1: 人性化鼠标移动
print("\n--- 演示 1: 人性化鼠标移动 ---")
print("鼠标将沿贝塞尔曲线移动到屏幕中心...")

local centerX = screenWidth / 2
local centerY = screenHeight / 2

wingman.human.mouse_move(centerX, centerY)
wingman.util.sleep(500)

-- 演示 2: 点击
print("\n--- 演示 2: 点击 ---")
print("在中心位置点击...")
wingman.human.mouse_click(centerX, centerY)
wingman.util.sleep(1000)

-- 演示 3: 拖拽
print("\n--- 演示 3: 拖拽 ---")
print("从中心拖拽到右下角...")
wingman.human.mouse_drag(centerX, centerY, centerX + 200, centerY + 200)
wingman.util.sleep(1000)

-- 演示 4: 滚动
print("\n--- 演示 4: 滚动 ---")
print("向上滚动...")
wingman.human.mouse_scroll(centerX, centerY, -120)
wingman.util.sleep(500)

print("向下滚动...")
wingman.human.mouse_scroll(centerX, centerY, 120)
wingman.util.sleep(1000)

-- 演示 5: 双击
print("\n--- 演示 5: 双击 ---")
print("在中心位置双击...")
wingman.human.mouse_doubleClick(centerX, centerY)
wingman.util.sleep(1000)

-- 演示 6: 右键点击
print("\n--- 演示 6: 右键点击 ---")
print("右键点击...")
wingman.human.mouse_rightClick(centerX, centerY)
wingman.util.sleep(1000)

-- 演示 7: 键盘输入
print("\n--- 演示 7: 键盘输入 ---")
print("按 F 键...")
wingman.human.keyboard_press(0x46)  -- F key
wingman.util.sleep(1000)

-- 演示 8: 文本输入（带随机延迟）
print("\n--- 演示 8: 文本输入 ---")
print("输入文本 'Hello, World!'...")
wingman.human.keyboard_type("Hello, World!")
wingman.util.sleep(1000)

-- 演示 9: 文本输入（带随机大小写）
print("\n--- 演示 9: 文本输入（随机大小写）---")
print("输入文本 'random case'...")
wingman.human.keyboard_type("random case", true)
wingman.util.sleep(1000)

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
    wingman.human.mouse_click(pos.x, pos.y)
    wingman.util.sleep(500)
end

print("\n=== 示例完成 ===")
print("提示: 可以调整 wingman.human.setConfig(key, value) 参数来改变行为")
