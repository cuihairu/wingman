# Tray API

系统托盘图标模块，用于在 Windows 任务栏显示托盘图标和弹出菜单。

## Lua API

### tray.create(tooltip)

创建一个新的托盘图标。

```lua
local icon = tray.create("Wingman")
```

**参数:**
- `tooltip` - 鼠标悬停时显示的提示文本

**返回:** TrayIcon 对象

---

### tray.get(id)

获取已创建的托盘图标。

```lua
local icon = tray.get("main")
```

---

### tray.remove(id)

移除托盘图标。

```lua
tray.remove("main")
```

## TrayIcon 对象方法

### icon:setIcon(iconPath)

设置托盘图标的图标文件。

```lua
icon:setIcon("C:/path/to/icon.ico")
```

**参数:**
- `iconPath` - .ico 文件路径

---

### icon:setTooltip(tooltip)

设置鼠标悬停提示文本。

```lua
icon:setTooltip("Wingman 自动化引擎")
```

---

### icon:addItem(id, label, callback)

添加菜单项。

```lua
icon:addItem("start", "启动脚本", function()
    print("启动脚本!")
end)
```

**参数:**
- `id` - 菜单项唯一标识
- `label` - 显示文本
- `callback` - 点击时调用的函数（可选）

---

### icon:addSeparator(id)

添加分隔线。

```lua
icon:addSeparator("sep1")
```

---

### icon:addSubmenu(id, label, items)

添加子菜单。

```lua
icon:addSubmenu("scripts", "脚本", {
    {id = "s1", label = "脚本1"},
    {id = "s2", label = "脚本2"}
})
```

---

### icon:removeItem(id)

移除菜单项。

```lua
icon:removeItem("start")
```

---

### icon:clearItems()

清空所有菜单项。

```lua
icon:clearItems()
```

---

### icon:show()

显示托盘图标。

```lua
icon:show()
```

---

### icon:hide()

隐藏托盘图标。

```lua
icon:hide()
```

---

### icon:updateMenu()

更新菜单。

```lua
icon:updateMenu()
```

---

### icon:isVisible()

检查托盘图标是否可见。

```lua
if icon:isVisible() then
    print("图标可见")
end
```

**返回:** boolean

---

### icon:destroy()

销毁托盘图标。

```lua
icon:destroy()
```

## 完整示例

```lua
-- 创建托盘图标
local icon = tray.create("Wingman")

-- 设置图标
icon:setIcon("assets/icon.ico")

-- 添加菜单项
icon:addItem("start", "启动脚本", function()
    print("启动脚本...")
end)

icon:addItem("stop", "停止脚本", function()
    print("停止脚本...")
end)

-- 添加分隔线
icon:addSeparator("sep1")

-- 添加子菜单
icon:addSubmenu("tools", "工具", {
    {id = "t1", label = "录制宏"},
    {id = "t2", label = "查看日志"}
})

-- 添加退出项
icon:addItem("exit", "退出", function()
    os.exit()
end)

-- 显示图标
icon:show()

-- 保持运行
while true do
    util.sleep(1000)
end
```

## C++ API

### 创建托盘图标

```cpp
#include "wingman/tray.hpp"

// 创建托盘图标
auto icon = std::make_shared<TrayIcon>("Wingman");

// 设置图标
icon->setIcon("assets/icon.ico");

// 添加菜单项
TrayItem startItem;
startItem.id = "start";
startItem.label = "启动脚本";
startItem.callback = []() {
    std::cout << "启动脚本" << std::endl;
};
icon->addItem(startItem);

// 显示图标
icon->show();
```

### 使用 TrayManager

```cpp
// 创建并注册图标
auto icon = TrayManager::instance().createIcon("main", "Wingman");
icon->show();

// 获取图标
auto icon = TrayManager::instance().getIcon("main");

// 移除图标
TrayManager::instance().removeIcon("main");
```

## 注意事项

1. 图标文件必须是 .ico 格式
2. 菜单回调在 UI 线程执行，避免耗时操作
3. 托盘图标需要在消息循环中响应，确保程序保持运行
