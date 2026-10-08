-- Wingman Hello World 示例
-- 演示基本 API 使用

local wingman = require("wingman")

print("=== Wingman Hello World ===")

-- 获取屏幕尺寸
local width = wingman.screen.getScreenWidth()
local height = wingman.screen.getScreenHeight()
print(string.format("屏幕尺寸: %dx%d", width, height))

-- 获取前台窗口标题
local hwnd = wingman.window.getForeground()
if hwnd and hwnd ~= 0 then
    local title = wingman.window.getTitle(hwnd)
    print(string.format("前台窗口: %s", title))
end

-- 查找进程（返回数组：result[1]=PID或nil，result[2]=是否找到）
local result = wingman.process.find("chrome.exe")
if result[2] then
    print(string.format("Chrome 进程 PID: %d", result[1]))
else
    print("Chrome 未运行")
end

print("=== 完成 ===")
