-- Wingman Hello World 示例
-- 演示基本 API 使用

print("=== Wingman Hello World ===")

-- 获取屏幕尺寸
local width = screen.getScreenWidth()
local height = screen.getScreenHeight()
print(string.format("屏幕尺寸: %dx%d", width, height))

-- 获取鼠标位置
local pos = input.getMousePosition()
print(string.format("鼠标位置: (%d, %d)", pos.x, pos.y))

-- 获取前台窗口标题
local hwnd = window.getForeground()
if hwnd then
    local title = window.getTitle(hwnd)
    print(string.format("前台窗口: %s", title))
end

-- 列出所有进程
local chromePid = process.find("chrome.exe")
if chromePid ~= 0 then
    print(string.format("Chrome 进程 PID: %d", chromePid))
else
    print("Chrome 未运行")
end

print("=== 完成 ===")
