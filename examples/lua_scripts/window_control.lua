-- 窗口控制示例
-- 演示如何操作窗口

local wingman = require("wingman")

print("=== 窗口控制示例 ===")

-- 列出所有窗口
print("正在枚举窗口...")
-- 注意: wingman.window.enumerate() 需要在 C++ 中实现

-- 查找特定窗口
local targetTitle = "Notepad"  -- 记事本
print("正在查找窗口: " .. targetTitle)

local hwnd, found = wingman.window.find(targetTitle)

if found then
    print("找到窗口!")

    -- 获取窗口标题
    local title = wingman.window.getTitle(hwnd)
    print("窗口标题: " .. title)

    -- 获取窗口位置和大小
    local bounds = wingman.window.getBounds(hwnd)
    print(string.format("窗口位置: %d, %d", bounds.x, bounds.y))
    print(string.format("窗口大小: %d x %d", bounds.width, bounds.height))

    -- 激活窗口
    print("激活窗口...")
    wingman.window.activate(hwnd)
    wingman.util.sleep(500)

    -- 移动窗口
    print("移动窗口到 100, 100...")
    wingman.window.move(hwnd, 100, 100)
    wingman.util.sleep(500)

    -- 调整窗口大小
    print("调整窗口大小为 800 x 600...")
    wingman.window.resize(hwnd, 800, 600)
    wingman.util.sleep(500)

else
    print("未找到窗口: " .. targetTitle)
    print("提示: 请先打开记事本窗口")
end

print("脚本执行完成!")
