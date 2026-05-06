# API: window

窗口管理模块。

## 函数

### find(title)

查找指定标题的窗口。

**参数：**
- `title` (string) - 窗口标题（支持部分匹配）

**返回：**
- `number` | `nil` - 窗口句柄 (HWND)，未找到返回 nil
- `boolean` - 是否找到

**示例：**
```lua
local hwnd, found = window.find("记事本")
if found then
    print("找到记事本窗口，句柄: " .. hwnd)
end
```

### activate(hwnd)

激活窗口（置顶）。

**参数：**
- `hwnd` (number) - 窗口句柄

**返回：**
- `boolean` - 是否成功

**示例：**
```lua
local hwnd, found = window.find("记事本")
if found then
    window.activate(hwnd)
end
```

### getForeground()

获取当前前台窗口。

**返回：**
- `number` - 前台窗口句柄 (HWND)

**示例：**
```lua
local hwnd = window.getForeground()
local title = window.getTitle(hwnd)
print("前台窗口: " .. title)
```

### getTitle(hwnd)

获取窗口标题。

**参数：**
- `hwnd` (number) - 窗口句柄

**返回：**
- `string` - 窗口标题

**示例：**
```lua
local hwnd = window.getForeground()
local title = window.getTitle(hwnd)
print("标题: " .. title)
```

### getBounds(hwnd)

获取窗口边界。

**参数：**
- `hwnd` (number) - 窗口句柄

**返回：**
- `table` - 包含以下字段：
    - `x` - X 坐标
    - `y` - Y 坐标
    - `width` - 宽度
    - `height` - 高度

**示例：**
```lua
local hwnd, found = window.find("记事本")
if found then
    local bounds = window.getBounds(hwnd)
    print(string.format("位置: (%d, %d), 大小: %dx%d",
        bounds.x, bounds.y, bounds.width, bounds.height))
end
```

### waitFor(title, timeout)

等待窗口出现。

**参数：**
- `title` (string) - 窗口标题（支持部分匹配）
- `timeout` (number) - 超时时间（毫秒），默认 5000

**返回：**
- `boolean` - 是否在超时前找到窗口

**示例：**
```lua
-- 启动应用程序
process.start("notepad.exe")

-- 等待窗口出现
if window.waitFor("记事本", 5000) then
    print("记事本已启动")
    local hwnd, found = window.find("记事本")
    if found then
        window.activate(hwnd)
    end
else
    print("超时：记事本未启动")
end
```

## 完整示例

```lua
-- 查找并激活记事本
local hwnd, found = window.find("记事本")
if not found then
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")

    -- 等待窗口出现
    if not window.waitFor("记事本", 3000) then
        print("启动失败")
        return
    end

    hwnd, found = window.find("记事本")
end

if found then
    -- 激活窗口
    window.activate(hwnd)
    util.sleep(200)

    -- 获取窗口信息
    local title = window.getTitle(hwnd)
    print("标题: " .. title)

    local bounds = window.getBounds(hwnd)
    print(string.format("位置: (%d, %d)", bounds.x, bounds.y))
    print(string.format("大小: %dx%d", bounds.width, bounds.height))
end
```

## 注意事项

1. 窗口句柄 (HWND) 是一个数字，在窗口生命周期内有效
2. 窗口关闭后，句柄变为无效
3. 某些应用程序可能阻止窗口激活操作
4. 管理员权限的应用可能需要管理员权限才能操作
