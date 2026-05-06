# API: uia

UI Automation 模块，用于与 Windows 应用程序进行自动化交互。

> 基于 Microsoft UI Automation API，支持与大部分 Windows 应用程序的控件进行交互。

## 概述

UI Automation (UIA) 允许脚本直接操作应用程序的 UI 控件，而不是依赖坐标点击。这使得自动化更加可靠，不受窗口位置或大小变化的影响。

### 支持的控件类型

- **Button** - 按钮
- **Edit** - 文本输入框
- **Text** - 静态文本
- **ComboBox** - 下拉框
- **List** - 列表
- **CheckBox** - 复选框
- **RadioButton** - 单选按钮
- **Tab** - 标签页
- **Menu** - 菜单
- **Window** - 窗口
- ... 以及更多 Windows 标准控件

## 函数

### fromForeground()

获取前台窗口的 UI Automation 根元素。

**返回：**
- `UIElement` | `nil` - 根元素对象，失败返回 nil

**示例：**
```lua
local root = uia.fromForeground()
if root then
    local info = root:getInfo()
    print("Foreground window: " .. info.name)
end
```

### fromWindow(hwnd)

从窗口句柄获取 UI Automation 根元素。

**参数：**
- `hwnd` (number) - 窗口句柄

**返回：**
- `UIElement` | `nil` - 根元素对象，失败返回 nil

**示例：**
```lua
local hwnd, found = window.find("记事本")
if found then
    local root = uia.fromWindow(hwnd)
    if root then
        print("记事本 UI 根元素获取成功")
    end
end
```

### fromPoint(x, y)

从屏幕坐标获取 UI 元素。

**参数：**
- `x` (number) - X 坐标
- `y` (number) - Y 坐标

**返回：**
- `UIElement` | `nil` - 元素对象，失败返回 nil

**示例：**
```lua
local x, y = input.getMousePos()
local element = uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print("Element: " .. info.name .. " (" .. info.controlType .. ")")
end
```

### findByName(name)

在前台窗口中查找指定名称的元素。

**参数：**
- `name` (string) - 元素名称

**返回：**
- `UIElement` | `nil` - 找到的元素，未找到返回 nil

**示例：**
```lua
local fileMenu = uia.findByName("文件")
if fileMenu then
    fileMenu:click()
end
```

### findById(id)

在前台窗口中查找指定 Automation ID 的元素。

**参数：**
- `id` (string) - Automation ID

**返回：**
- `UIElement` | `nil` - 找到的元素，未找到返回 nil

### findButton(name)

查找按钮控件。

**参数：**
- `name` (string) - 按钮名称（支持部分匹配）

**返回：**
- `UIElement` | `nil` - 找到的按钮，未找到返回 nil

**示例：**
```lua
local okBtn = uia.findButton("确定")
if okBtn then
    okBtn:click()
end
```

### findEdit(name)

查找编辑框控件。

**参数：**
- `name` (string) - 编辑框名称（支持部分匹配，空字符串匹配任意编辑框）

**返回：**
- `UIElement` | `nil` - 找到的编辑框，未找到返回 nil

**示例：**
```lua
local edit = uia.findEdit("")
if edit then
    edit:setValue("Hello, UI Automation!")
end
```

### findText(name)

查找文本控件。

**参数：**
- `name` (string) - 文本名称（支持部分匹配）

**返回：**
- `UIElement` | `nil` - 找到的文本元素，未找到返回 nil

### waitForName(name, timeout)

等待指定名称的元素出现。

**参数：**
- `name` (string) - 元素名称
- `timeout` (number) - 超时时间（毫秒），默认 5000

**返回：**
- `UIElement` | `nil` - 找到的元素，超时返回 nil

**示例：**
```lua
local dialog = uia.waitForName("对话框", 3000)
if dialog then
    print("对话框已出现")
end
```

## UIElement 对象

### 方法

#### getInfo()

获取元素的完整信息。

**返回：**
- `table` - 包含以下字段：
    - `name` - 元素名称
    - `className` - 类名
    - `automationId` - Automation ID
    - `controlType` - 控件类型
    - `bounds` - 边界矩形 {x, y, width, height}
    - `isEnabled` - 是否可用
    - `isVisible` - 是否可见

**示例：**
```lua
local info = element:getInfo()
print(string.format("Name: %s, Type: %s", info.name, info.controlType))
```

#### click()

点击元素。

**返回：**
- `boolean` - 是否成功

**示例：**
```lua
element:click()
```

#### rightClick()

右键点击元素。

**返回：**
- `boolean` - 是否成功

#### doubleClick()

双击元素。

**返回：**
- `boolean` - 是否成功

#### focus()

设置焦点到元素。

**返回：**
- `boolean` - 是否成功

#### getValue()

获取元素的值（适用于编辑框等）。

**返回：**
- `string` - 元素值

#### setValue(value)

设置元素的值。

**参数：**
- `value` (string) - 要设置的值

**返回：**
- `boolean` - 是否成功

**示例：**
```lua
local edit = uia.findEdit("")
edit:setValue("Hello World")
```

#### getName()

获取元素名称。

**返回：**
- `string` - 名称

#### getChildren()

获取所有子元素。

**返回：**
- `table` - UIElement 数组

**示例：**
```lua
local children = root:getChildren()
for i, child in ipairs(children) do
    local info = child:getInfo()
    print(string.format("[%d] %s - %s", i, info.name, info.controlType))
end
```

## 完整示例

```lua
-- 等待记事本窗口出现
print("等待记事本窗口...")
local hwnd, found = window.find("记事本")
if not found then
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
    -- 设置文本
    edit:setValue("Hello from UI Automation!\n这是通过 Lua 脚本输入的文本。\n")
    print("已设置文本")

    -- 获取文本
    local value = edit:getValue()
    print("当前文本长度: " .. #value)
end

-- 查找关闭按钮
print("\n=== 查找关闭按钮 ===")
local closeButton = uia.findButton("关闭")
if closeButton then
    print("找到关闭按钮")
    -- closeButton:click()  -- 取消注释可点击关闭按钮
end
```

## 注意事项

1. UI Automation 需要目标应用程序支持 UIA 接口，大部分现代 Windows 应用都支持
2. 某些使用自定义绘制的应用可能不完全支持 UIA
3. 如果 UIA 操作失败，可以降级使用坐标点击
4. 使用 `window.find()` 确保目标窗口在前台
