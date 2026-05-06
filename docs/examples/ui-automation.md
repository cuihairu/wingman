# UI Automation 示例

演示如何使用 UI Automation 操作 Windows 应用程序。

## 场景：自动化记事本

这个示例展示如何：
1. 查找并激活记事本窗口
2. 使用 UI Automation 直接操作编辑框输入文本
3. 获取编辑框内容

```lua
--[[
    UI Automation 示例 - 记事本自动化

    使用方法:
    1. 打开记事本 (notepad.exe)
    2. 运行此脚本: wingman.exe script scripts/ui_automation_example.lua
]]

-- 等待记事本窗口出现
print("等待记事本窗口...")
local hwnd, found = window.find("记事本")
if not found then
    -- 尝试创建新记事本
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")
    util.sleep(1000)
    hwnd, found = window.find("记事本")
end

if not found then
    print("错误: 无法找到记事本窗口")
    return
end

print("找到记事本窗口")

-- 激活窗口
window.activate(hwnd)
util.sleep(200)

-- 获取前台窗口的 UI Automation 根元素
local root = uia.fromForeground()
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
local edit = uia.findEdit("")
if edit then
    print("找到编辑框")
    print("编辑框名称: " .. edit:getName())

    -- 设置文本
    edit:setValue("Hello from UI Automation!\n这是通过 Lua 脚本输入的文本。\n")
    print("已设置文本")

    -- 等待一下
    util.sleep(500)

    -- 获取文本
    local value = edit:getValue()
    print("当前文本长度: " .. #value)
else
    print("未找到编辑框")
end

-- 查找菜单栏的"文件"按钮
print("\n=== 查找菜单 ===")
local fileMenu = uia.findByName("文件")
if fileMenu then
    print("找到文件菜单: " .. fileMenu:getName())
    -- fileMenu:click()  -- 取消注释可点击菜单
end

-- 从鼠标位置获取元素
print("\n=== 从鼠标位置获取元素 ===")
local x, y = input.getMousePos()
print(string.format("鼠标位置: (%d, %d)", x, y))
local element = uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print(string.format("鼠标下的元素: %s (%s)", info.name, info.controlType))
end

-- 查找记事本标题栏的关闭按钮
print("\n=== 查找关闭按钮 ===")
local closeButton = uia.findButton("关闭")
if closeButton then
    print("找到关闭按钮")
    -- closeButton:click()  -- 取消注释可点击关闭按钮
else
    print("未找到关闭按钮")
end

-- 等待特定元素出现
print("\n=== 等待元素测试 ===")
local testElement = uia.waitForName("不存在的元素", 2000)
if testElement then
    print("元素已出现")
else
    print("元素超时未出现（预期行为）")
end

print("\n=== UI Automation 测试完成 ===")
```

## 场景：表单自动填写

```lua
-- 假设有一个表单窗口，包含姓名、邮箱输入框和提交按钮

-- 查找表单窗口
local hwnd, found = window.find("用户注册")
if found then
    window.activate(hwnd)
    util.sleep(300)

    -- 查找并填写姓名输入框
    local nameEdit = uia.findEdit("姓名")
    if nameEdit then
        nameEdit:setValue("张三")
    end

    -- 查找并填写邮箱输入框
    local emailEdit = uia.findEdit("邮箱")
    if emailEdit then
        emailEdit:setValue("zhangsan@example.com")
    end

    -- 点击提交按钮
    local submitBtn = uia.findButton("提交")
    if submitBtn then
        submitBtn:click()
    end
end
```

## 场景：遍历菜单项

```lua
local root = uia.fromForeground()
if root then
    -- 查找菜单栏
    local children = root:getChildren()
    for _, child in ipairs(children) do
        local info = child:getInfo()
        if info.controlType == "MenuBar" or info.className == "MenuBar" then
            print("找到菜单栏")
            local menuItems = child:getChildren()
            for _, item in ipairs(menuItems) do
                print(string.format("  菜单项: %s", item:getName()))
            end
        end
    end
end
```

## 场景：事件监听器

监听 UI 元素的变化并做出响应。

```lua
-- 监听编辑框内容变化
local editListener = uia.onPropertyChanged("编辑框", function(propertyName, value)
    if propertyName == "Value" then
        print("编辑框内容已变化: " .. value)
    end
end)

if editListener then
    print("编辑框监听器已注册，ID: " .. editListener)
end

-- 监听列表结构变化（例如项目添加/删除）
local listListener = uia.onStructureChanged("列表", function()
    print("列表结构已变化")
    -- 可以重新获取列表项
    local list = uia.findByName("列表")
    if list then
        local items = list:getChildren()
        print("当前列表项数量: " .. #items)
    end
end)

if listListener then
    print("列表监听器已注册，ID: " .. listListener)
end

-- 运行一段时间后移除监听器
util.sleep(10000)  -- 监听 10 秒

if editListener then
    uia.removeEventListener(editListener)
    print("编辑框监听器已移除")
end

if listListener then
    uia.removeEventListener(listListener)
    print("列表监听器已移除")
end
```

### 监听对话框出现并自动点击

```lua
-- 监听确认对话框的出现
local dialogListener = uia.onPropertyChanged("确认", function(propertyName, value)
    if propertyName == "Name" and value == "确认" then
        print("检测到确认对话框")

        -- 自动点击"是"按钮
        util.sleep(200)
        local yesBtn = uia.findButton("是")
        if yesBtn then
            yesBtn:click()
            print("已自动点击"是"按钮")
        end
    end
end)

if dialogListener then
    print("对话框监听器已注册，等待对话框出现...")
end

-- 等待 30 秒或直到对话框出现
util.sleep(30000)

if dialogListener then
    uia.removeEventListener(dialogListener)
end
```

## 场景：等待对话框并点击

```lua
-- 执行某个操作后，等待确认对话框出现
-- ...

-- 等待对话框
local dialog = uia.waitForName("确认", 5000)
if dialog then
    -- 查找"是"按钮并点击
    local yesBtn = uia.findButton("是")
    if yesBtn then
        yesBtn:click()
    end
end
```

## 运行示例

```bash
wingman.exe script scripts/ui_automation_example.lua
```

## 调试技巧

1. **使用 `fromPoint` 查看鼠标下的元素**
```lua
local x, y = input.getMousePos()
local element = uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print(string.format("Name: %s, Type: %s, Class: %s",
        info.name, info.controlType, info.className))
end
```

2. **遍历所有子元素了解窗口结构**
```lua
local root = uia.fromForeground()
if root then
    local function printTree(element, depth)
        local indent = string.rep("  ", depth)
        local info = element:getInfo()
        print(string.format("%s[%s] %s", indent, info.controlType, info.name))

        local children = element:getChildren()
        for _, child in ipairs(children) do
            printTree(child, depth + 1)
        end
    end

    printTree(root, 0)
end
```

## 注意事项

1. UI Automation 依赖目标应用的 UIA 支持，大部分 Windows 应用都支持
2. 某些使用自定义绘制的应用可能不完全支持
3. 操作前确保窗口已激活且可见
4. 某些操作可能需要管理员权限
