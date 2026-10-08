-- 窗口控制示例
-- 演示如何操作窗口

local wingman = require("wingman")

print("=== 窗口控制示例 ===")

-- 列出所有窗口
print("正在枚举窗口...")
local windows = wingman.node.getWindows()
for i, w in ipairs(windows) do
    local mark = w.isForeground and " [前台]" or ""
    print(string.format("  [%d] %s (HWND: %d)%s", i, w.title, w.handle, mark))
end

-- 查找特定窗口
local targetTitle = "Notepad"  -- 记事本
print("正在查找窗口: " .. targetTitle)

local result = wingman.window.find(targetTitle)

if result[2] then
    local hwnd = result[1]
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

    -- 重新读取窗口边界
    bounds = wingman.window.getBounds(hwnd)
    print(string.format("激活后窗口位置: %d, %d", bounds.x, bounds.y))

    -- 注意: 现行 window 模块未提供 move/resize 接口
    -- （可用能力: find/activate/getForeground/getTitle/getBounds/waitFor）
else
    print("未找到窗口: " .. targetTitle)
    print("提示: 请先打开记事本窗口")
end

print("脚本执行完成!")
