# UI Automation 示例

演示如何使用 UI Automation 操作 Windows 应用程序。

`wingman.uia` 模块 API 全部为 **snake_case**（Lua 与 Python 同名，Python 侧另有 camelCase 别名但仅限模块函数）；元素对象是 table/dict，方法请用**点号调用**（`element.click()`），不要用冒号——冒号会把元素自身作为第一个参数挤占形参位。

## 可用 API 速查

模块函数：

| 分类 | 函数 |
|------|------|
| 根元素 | `from_foreground()`、`from_point(x, y)`、`from_window(hwnd)` |
| 通用查找 | `find_by_name(name)`、`find_by_id(id)`、`find_all_by_control_type(role:int)`、`wait_for_name(name, timeout?)`、`wait_for_id(id, timeout?)`、`wait_for_role(control_type, timeout?)`、`wait_for(selector, timeout?)` |
| 专用查找 | `find_button(name)`、`find_edit(name)`、`find_text(name)`、`find_check_box(name)`、`find_radio_button(name)`、`find_combo_box(name)`、`find_list(name)`、`find_list_item(name)`、`find_tree(name)`、`find_tree_item(name)`、`find_menu_item(name)`、`find_hyperlink(name)`、`find_image(name)`、`find_slider(name)`、`find_spinner(name)`、`find_progress_bar(name)`、`find_tab(name)`、`find_tab_item(name)` |
| 事件监听 | `on_property_changed(name, callback)`、`on_structure_changed(name, callback)`、`remove_event_listener(id)`（**Python 专用**，Lua 回调会被拒绝） |

元素对象方法：`get_info()`、`click()`、`right_click()`、`double_click()`、`focus()`、`get_value()`、`set_value(text)`、`get_children()`、`get_parent()`、`expand()`、`collapse()`、`is_expanded()`、`is_visible()`、`is_enabled()`、`select()`、`get_selection()`、`is_checked()`、`set_checked(checked)`。

`get_info()` 返回字段：`name`、`id`、`className`、`role`（**int** 控件类型，无 `controlType` 字符串）、`text`、`is_enabled`、`is_visible`、`has_focus`、`bounds`。

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
    2. 运行此脚本: wingman-runtime.exe script examples/lua_scripts/ui_automation_example.lua
]]

local wingman = require("wingman")

-- 等待记事本窗口出现（window.find 返回单数组：res[1] = 窗口句柄，res[2] = 是否找到）
print("等待记事本窗口...")
local res = wingman.window.find("记事本")
if not res[2] then
    -- 尝试创建新记事本
    print("未找到记事本，尝试启动...")
    wingman.process.start("notepad.exe")
    wingman.util.sleep(1000)
    res = wingman.window.find("记事本")
end

if not res[2] then
    print("错误: 无法找到记事本窗口")
    return
end

local hwnd = res[1]
print("找到记事本窗口")

-- 激活窗口
wingman.window.activate(hwnd)
wingman.util.sleep(200)

-- 获取前台窗口的 UI Automation 根元素
local root = wingman.uia.from_foreground()
if not root then
    print("错误: 无法获取 UI Automation 元素")
    return
end

print("UI Automation 已初始化")

-- 获取所有子元素
print("\n=== 记事本 UI 元素 ===")
local children = root.get_children()
for i, child in ipairs(children) do
    local info = child.get_info()
    print(string.format("[%d] %s - %s (role: %d, 可见: %s)",
        i, info.name, info.className, info.role,
        info.is_visible and "是" or "否"))
end

-- 查找编辑框
print("\n=== 查找编辑框 ===")
local edit = wingman.uia.find_edit("")
if edit then
    print("找到编辑框")
    print("编辑框名称: " .. edit.get_info().name)

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

-- 查找菜单栏的"文件"项
print("\n=== 查找菜单 ===")
local fileMenu = wingman.uia.find_by_name("文件")
if fileMenu then
    print("找到文件菜单: " .. fileMenu.get_info().name)
    -- fileMenu.click()  -- 取消注释可点击菜单
end

-- 从屏幕坐标取元素（脚本层没有获取鼠标当前位置的 API，坐标需自行确定）
print("\n=== 从坐标取元素 ===")
local element = wingman.uia.from_point(400, 300)
if element then
    local info = element.get_info()
    print(string.format("该坐标下的元素: %s (%s)", info.name, info.className))
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
```

## 场景：表单自动填写

```lua
local wingman = require("wingman")

-- 假设有一个表单窗口，包含姓名、邮箱输入框和提交按钮

-- 查找表单窗口
local res = wingman.window.find("用户注册")
if res[2] then
    wingman.window.activate(res[1])
    wingman.util.sleep(300)

    -- 查找并填写姓名输入框
    local nameEdit = wingman.uia.find_edit("姓名")
    if nameEdit then
        nameEdit.set_value("张三")
    end

    -- 查找并填写邮箱输入框
    local emailEdit = wingman.uia.find_edit("邮箱")
    if emailEdit then
        emailEdit.set_value("zhangsan@example.com")
    end

    -- 点击提交按钮
    local submitBtn = wingman.uia.find_button("提交")
    if submitBtn then
        submitBtn.click()
    end
end
```

## 场景：遍历菜单项

```lua
local wingman = require("wingman")

local root = wingman.uia.from_foreground()
if root then
    -- 控件类型是数字 role；这里按 className 识别菜单栏
    local children = root.get_children()
    for _, child in ipairs(children) do
        local info = child.get_info()
        if info.className == "MenuBar" then
            print("找到菜单栏")
            local menuItems = child.get_children()
            for _, item in ipairs(menuItems) do
                print(string.format("  菜单项: %s", item.get_info().name))
            end
        end
    end
end
```

## 场景：事件监听（Python 专用）

UIA 事件回调从后台线程触发，需要线程安全的 callable。**Lua callable 非线程安全，`on_property_changed` / `on_structure_changed` 传 Lua 函数会返回 0 并拒绝注册**（同时广播 `uia.error` 事件），因此事件监听场景请使用 Python（Python callable 持 GIL，可安全跨线程回调）：

```python
# 需启用 Python 引擎的构建（WINGMAN_ENABLE_PYTHON=ON）
import wingman

# 监听编辑框内容变化：callback(元素name, 元素当前文本)
edit_listener = wingman.uia.on_property_changed(
    "编辑框",
    lambda name, text: print(f"编辑框内容已变化: {name} -> {text}"))
print(f"编辑框监听器已注册，ID: {edit_listener}")

# 监听列表结构变化（例如项目添加/删除）：callback 无参数
list_listener = wingman.uia.on_structure_changed(
    "列表",
    lambda: print("列表结构已变化"))
print(f"列表监听器已注册，ID: {list_listener}")

# 运行一段时间后移除监听器
wingman.util.sleep(10000)  # 监听 10 秒

wingman.uia.remove_event_listener(edit_listener)
wingman.uia.remove_event_listener(list_listener)
print("监听器已移除")
```

### 监听对话框出现并自动点击（Python）

```python
import wingman

# 监听确认对话框的出现
def on_dialog(name, text):
    if name == "确认":
        print("检测到确认对话框")

        # 自动点击"是"按钮
        wingman.util.sleep(200)
        yes_btn = wingman.uia.find_button("是")
        if yes_btn:
            yes_btn["click"]()
            print('已自动点击"是"按钮')

dialog_listener = wingman.uia.on_property_changed("确认", on_dialog)
print("对话框监听器已注册，等待对话框出现...")

# 等待 30 秒或直到对话框触发
wingman.util.sleep(30000)

wingman.uia.remove_event_listener(dialog_listener)
```

> 注意：元素对象在 Python 侧是 **dict**（C++ Object 直接映射为 dict），方法以 `element["click"]()` 形式调用；`get_info()` 返回的同样是 dict（`info["name"]`）。

## 场景：等待对话框并点击

```lua
local wingman = require("wingman")

-- 执行某个操作后，等待确认对话框出现
-- ...

-- 等待对话框（wait_for_name(name, timeout)，timeout 默认 3000ms）
local dialog = wingman.uia.wait_for_name("确认", 5000)
if dialog then
    -- 查找"是"按钮并点击
    local yesBtn = wingman.uia.find_button("是")
    if yesBtn then
        yesBtn.click()
    end
end
```

## 运行示例

```bash
wingman-runtime.exe script examples/lua_scripts/ui_automation_example.lua
```

## 调试技巧

1. **使用 `from_point` 查看坐标下的元素**
```lua
local wingman = require("wingman")

-- 脚本层没有取鼠标位置的 API，坐标需自行确定
local element = wingman.uia.from_point(400, 300)
if element then
    local info = element.get_info()
    print(string.format("Name: %s, Role: %d, Class: %s",
        info.name, info.role, info.className))
end
```

2. **遍历所有子元素了解窗口结构**
```lua
local wingman = require("wingman")

local root = wingman.uia.from_foreground()
if root then
    local function printTree(element, depth)
        local indent = string.rep("  ", depth)
        local info = element.get_info()
        print(string.format("%s[%s] %s", indent, info.className, info.name))

        local children = element.get_children()
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
3. 控件类型是数字 `role`（`find_all_by_control_type(role)` 按其过滤），`get_info()` 没有 `controlType` 字符串字段
4. 元素方法用点号调用（`element.click()`）；冒号调用会把元素自身挤占第一个形参（`set_value` 等带参方法会因此失效）
5. 操作前确保窗口已激活且可见
6. 某些操作可能需要管理员权限
