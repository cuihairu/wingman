--[[
    UI Automation 示例脚本

    演示如何使用 UI Automation 操作 Windows 应用程序

    使用方法:
    1. 打开记事本 (notepad.exe)
    2. 运行此脚本: wingman.exe script scripts/ui_automation_example.lua
]]

local wingman = require("wingman")

-- 等待记事本窗口出现
print("等待记事本窗口...")
local hwnd, found = wingman.window.find("记事本")
if not found then
    -- 尝试创建新记事本
    print("未找到记事本，尝试启动...")
    wingman.process.start("notepad.exe")
    wingman.util.sleep(1000)
    hwnd, found = wingman.window.find("记事本")
end

if not found then
    print("错误: 无法找到记事本窗口")
    return
end

print("找到记事本窗口")

-- 获取前台窗口的 UI Automation 根元素
local root = wingman.uia.fromForeground()
if not root then
    print("错误: 无法获取 UI Automation 元素")
    return
end

print("UI Automation 已初始化")

-- 获取所有子元素
print("\n=== 记事本 UI 元素 ===")
local children = root:getChildren()
for i, child in ipairs(children) do
    local info = child:getInfo()
    print(string.format("[%d] %s - %s (类型: %s, 可见: %s)",
        i, info.name, info.className, info.controlType,
        info.isVisible and "是" or "否"))
end

-- 查找编辑框
print("\n=== 查找编辑框 ===")
local edit = wingman.uia.findEdit("")
if edit then
    print("找到编辑框")
    print("编辑框名称: " .. edit:getName())

    -- 设置文本
    edit:setValue("Hello from UI Automation!\n这是通过 Lua 脚本输入的文本。\n")
    print("已设置文本")

    -- 等待一下
    wingman.util.sleep(500)

    -- 获取文本
    local value = edit:getValue()
    print("当前文本长度: " .. #value)
else
    print("未找到编辑框")
end

-- 查找菜单栏的"文件"按钮
print("\n=== 查找菜单 ===")
local fileMenu = wingman.uia:findByName("文件")
if fileMenu then
    print("找到文件菜单: " .. fileMenu:getName())
    -- fileMenu:click()  -- 取消注释可点击菜单
end

-- 从鼠标位置获取元素
print("\n=== 从鼠标位置获取元素 ===")
local x, y = wingman.input.getMousePos()
print(string.format("鼠标位置: (%d, %d)", x, y))
local element = wingman.uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print(string.format("鼠标下的元素: %s (%s)", info.name, info.controlType))
end

-- 查找记事本标题栏的关闭按钮
print("\n=== 查找关闭按钮 ===")
local closeButton = wingman.uia.findButton("关闭")
if closeButton then
    print("找到关闭按钮")
    -- closeButton:click()  -- 取消注释可点击关闭按钮
else
    print("未找到关闭按钮")
end

-- 等待特定元素出现
print("\n=== 等待元素测试 ===")
local testElement = wingman.uia:waitForName("不存在的元素", 2000)
if testElement then
    print("元素已出现")
else
    print("元素超时未出现（预期行为）")
end

print("\n=== UI Automation 测试完成 ===")
