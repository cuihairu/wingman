-- Wingman 窗口操作示例
-- 演示窗口查找、激活、移动等操作

print("=== 窗口操作示例 ===")

-- 获取前台窗口
local function getForegroundWindow()
    local hwnd = window.getForeground()
    if hwnd then
        local title = window.getTitle(hwnd)
        local bounds = window.getBounds(hwnd)
        print(string.format("前台窗口: %s", title))
        print(string.format("  位置: (%d, %d)", bounds.x, bounds.y))
        print(string.format("  大小: %dx%d", bounds.width, bounds.height))
        return hwnd, title
    end
    return nil, nil
end

-- 查找窗口
local function findWindow(titlePattern)
    print(string.format("正在查找窗口: %s", titlePattern))
    local hwnd = window.find(titlePattern)
    if hwnd ~= 0 then
        print(string.format("找到窗口! HWND: %d", hwnd))
        return hwnd
    else
        print("未找到窗口")
        return 0
    end
end

-- 等待窗口出现
local function waitForWindow(titlePattern, timeout)
    timeout = timeout or 10000  -- 默认 10 秒
    print(string.format("等待窗口出现: %s (超时: %dms)", titlePattern, timeout))

    local hwnd = window.waitFor(titlePattern, timeout)
    if hwnd ~= 0 then
        print(string.format("窗口已出现! HWND: %d", hwnd))
        return hwnd
    else
        print("等待超时")
        return 0
    end
end

-- 移动和调整窗口大小
local function moveWindow(hwnd, x, y, width, height)
    if window.setBounds(hwnd, x, y, width, height) then
        print(string.format("窗口已移动到 (%d, %d), 大小 %dx%d", x, y, width, height))
        return true
    else
        print("移动窗口失败")
        return false
    end
end

-- 激活窗口
local function activateWindow(hwnd)
    if window.activate(hwnd) then
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
        process.start("notepad.exe")
        util.sleep(1000)
        hwnd = waitForWindow("Notepad", 5000)
    end

    if hwnd ~= 0 then
        -- 激活记事本
        activateWindow(hwnd)

        -- 获取当前位置
        local bounds = window.getBounds(hwnd)
        print(string.format("记事本当前位置: (%d, %d)", bounds.x, bounds.y))

        -- 移动到指定位置
        moveWindow(hwnd, 100, 100, 600, 400)

        -- 等待用户查看
        util.sleep(2000)

        -- 恢复原位置
        moveWindow(hwnd, bounds.x, bounds.y, bounds.width, bounds.height)
    end
end

-- 示例：遍历所有顶层窗口
local function listAllWindows()
    print("\n--- 列出所有窗口 ---")

    -- 这个功能需要 C++ 支持
    -- 暂时用前台窗口演示
    local hwnd = window.getForeground()
    if hwnd then
        print(string.format("当前前台: %s", window.getTitle(hwnd)))
    end
end

-- 运行示例
getForegroundWindow()
notepadExample()
listAllWindows()

print("=== 完成 ===")
