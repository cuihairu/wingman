-- Wingman 窗口操作示例
-- 演示窗口查找、激活、移动等操作

local wingman = require("wingman")

print("=== 窗口操作示例 ===")

-- 获取前台窗口
local function getForegroundWindow()
    local hwnd = wingman.window.getForeground()
    if hwnd and hwnd ~= 0 then
        local title = wingman.window.getTitle(hwnd)
        local bounds = wingman.window.getBounds(hwnd)
        print(string.format("前台窗口: %s", title))
        print(string.format("  位置: (%d, %d)", bounds.x, bounds.y))
        print(string.format("  大小: %dx%d", bounds.width, bounds.height))
        return hwnd, title
    end
    return nil, nil
end

-- 查找窗口（find 返回数组：result[1]=句柄或nil，result[2]=是否找到）
local function findWindow(titlePattern)
    print(string.format("正在查找窗口: %s", titlePattern))
    local result = wingman.window.find(titlePattern)
    if result[2] then
        local hwnd = result[1]
        print(string.format("找到窗口! HWND: %d", hwnd))
        return hwnd
    else
        print("未找到窗口")
        return 0
    end
end

-- 等待窗口出现（waitFor 返回是否在超时内出现，成功后再 find 取句柄）
local function waitForWindow(titlePattern, timeout)
    timeout = timeout or 10000  -- 默认 10 秒
    print(string.format("等待窗口出现: %s (超时: %dms)", titlePattern, timeout))

    if wingman.window.waitFor(titlePattern, timeout) then
        local result = wingman.window.find(titlePattern)
        local hwnd = result[2] and result[1] or 0
        print(string.format("窗口已出现! HWND: %d", hwnd))
        return hwnd
    else
        print("等待超时")
        return 0
    end
end

-- 激活窗口
local function activateWindow(hwnd)
    if wingman.window.activate(hwnd) then
        print("窗口已激活")
        return true
    else
        print("激活窗口失败")
        return false
    end
end

-- 示例：记事本操作
local function notepadExample()
    print("\n--- 记事本示例 ---")

    -- 查找记事本
    local hwnd = findWindow("Notepad")
    if hwnd == 0 then
        print("记事本未运行，尝试启动...")
        wingman.process.start("notepad.exe")
        wingman.util.sleep(1000)
        hwnd = waitForWindow("Notepad", 5000)
    end

    if hwnd ~= 0 then
        -- 激活记事本
        activateWindow(hwnd)

        -- 获取当前位置和大小
        local bounds = wingman.window.getBounds(hwnd)
        print(string.format("记事本当前位置: (%d, %d)", bounds.x, bounds.y))
        print(string.format("记事本当前大小: %dx%d", bounds.width, bounds.height))

        -- 注意: 现行 window 模块未提供 setBounds/move/resize 接口，这里仅读取展示
        wingman.util.sleep(2000)
    end
end

-- 示例：遍历所有顶层窗口
local function listAllWindows()
    print("\n--- 列出所有窗口 ---")

    local windows = wingman.node.getWindows()
    for i, w in ipairs(windows) do
        local mark = w.isForeground and " [前台]" or ""
        print(string.format("  [%d] %s (HWND: %d)%s", i, w.title, w.handle, mark))
    end
end

-- 运行示例
getForegroundWindow()
notepadExample()
listAllWindows()

print("=== 完成 ===")
