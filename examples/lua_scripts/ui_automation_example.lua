--[[
    UI Automation 示例脚本

    演示如何使用 UI Automation 操作 Windows 应用程序

    使用方法:
    1. 打开记事本 (notepad.exe)
    2. 运行此脚本: wingman.exe script scripts/ui_automation_example.lua
]]

local wingman = require("wingman")

-- 等待记事本窗口出现（find 返回数组：result[1]=句柄或nil，result[2]=是否找到）
print("等待记事本窗口...")
local result = wingman.window.find("记事本")
if not result[2] then
    -- 尝试创建新记事本
    print("未找到记事本，尝试启动...")
    wingman.process.start("notepad.exe")
    wingman.util.sleep(1000)
    result = wingman.window.find("记事本")
end

if not result[2] then
    print("错误: 无法找到记事本窗口")
    return
end

local hwnd = result[1]
print(string.format("找到记事本窗口 (HWND: %d)", hwnd))

-- 获取前台窗口的 UI Automation 根元素
local root = wingman.uia.from_foreground()
if not root then
    print("错误: 无法获取 UI Automation 元素")
    return
end

print("UI Automation 已初始化")

-- 获取所有子元素（元素方法用点号调用）
print("\n=== 记事本 UI 元素 ===")
local children = root.get_children()
for i, child in ipairs(children) do
    local info = child.get_info()
    print(string.format("[%d] %s - %s (角色: %d, 可见: %s)",
        i, info.name, info.className, info.role,
        info.is_visible and "是" or "否"))
end

-- 查找编辑框
print("\n=== 查找编辑框 ===")
local edit = wingman.uia.find_edit("")
if edit then
    print("找到编辑框: " .. edit.get_info().name)

    -- 设置文本
    edit.set_value("Hello from UI Automation!\n这是通过 Lua 脚本输入的文本。\n")
    print("已设置文本")

    -- 等待一下
    wingman.util.sleep(500)

    -- 获取文本
    local value = edit.get_value()
    print("当前文本长度: " .. #value)
else
    print("未找到编辑框")
end

-- 查找菜单栏的"文件"菜单
print("\n=== 查找菜单 ===")
local fileMenu = wingman.uia.find_by_name("文件")
if fileMenu then
    print("找到文件菜单: " .. fileMenu.get_info().name)
    -- fileMenu.click()  -- 取消注释可点击菜单
end

-- 从屏幕中心获取元素（input 模块无鼠标位置查询，取屏幕中心演示）
print("\n=== 从屏幕中心获取元素 ===")
local cx = math.floor(wingman.screen.getScreenWidth() / 2)
local cy = math.floor(wingman.screen.getScreenHeight() / 2)
print(string.format("屏幕中心: (%d, %d)", cx, cy))
local element = wingman.uia.from_point(cx, cy)
if element then
    local info = element.get_info()
    print(string.format("屏幕中心下的元素: %s (角色: %d)", info.name, info.role))
end

-- 查找记事本标题栏的关闭按钮
print("\n=== 查找关闭按钮 ===")
local closeButton = wingman.uia.find_button("关闭")
if closeButton then
    print("找到关闭按钮")
    -- closeButton.click()  -- 取消注释可点击关闭按钮
else
    print("未找到关闭按钮")
end

-- 等待特定元素出现
print("\n=== 等待元素测试 ===")
local testElement = wingman.uia.wait_for_name("不存在的元素", 2000)
if testElement then
    print("元素已出现")
else
    print("元素超时未出现（预期行为）")
end

print("\n=== UI Automation 测试完成 ===")
