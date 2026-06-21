-- Wingman 进程管理示例
-- 演示进程查找、启动、等待、终止等操作

local wingman = require("wingman")

print("=== 进程管理示例 ===")

-- 查找进程
local function findProcess(name)
    print(string.format("查找进程: %s", name))
    local pid = wingman.process.find(name)
    if pid ~= 0 then
        print(string.format("找到进程! PID: %d", pid))
        -- 获取进程路径
        local path = wingman.process.getPath(pid)
        if path and path ~= "" then
            print(string.format("路径: %s", path))
        end
        return pid
    else
        print("未找到进程")
        return 0
    end
end

-- 启动进程
local function startProcess(path, args)
    args = args or ""
    print(string.format("启动进程: %s %s", path, args))
    local pid = wingman.process.start(path, args)
    if pid ~= 0 then
        print(string.format("进程已启动! PID: %d", pid))
        return pid
    else
        print("启动进程失败")
        return 0
    end
end

-- 等待进程启动
local function waitForProcess(name, timeout)
    timeout = timeout or 10000
    print(string.format("等待进程启动: %s (超时: %dms)", name, timeout))
    local pid = wingman.process.waitFor(name, timeout)
    if pid ~= 0 then
        print(string.format("进程已启动! PID: %d", pid))
        return pid
    else
        print("等待超时")
        return 0
    end
end

-- 等待进程退出
local function waitForExit(pid, timeout)
    timeout = timeout or 30000
    print(string.format("等待进程退出: PID=%d (超时: %dms)", pid, timeout))
    local result = wingman.process.wait(pid, timeout)
    if result then
        print("进程已退出")
        return true
    else
        print("等待超时或进程不存在")
        return false
    end
end

-- 终止进程
local function terminateProcess(pid)
    print(string.format("终止进程: PID=%d", pid))
    if wingman.process.terminate(pid) then
        print("进程已终止")
        return true
    else
        print("终止进程失败")
        return false
    end
end

-- 检查进程是否存在
local function processExists(pid)
    local exists = wingman.process.exists(pid)
    print(string.format("进程 PID=%d %s", pid, exists and "存在" or "不存在"))
    return exists
    end

-- 示例：记事本操作
local function notepadExample()
    print("\n--- 记事本进程示例 ---")

    -- 查找记事本
    local pid = findProcess("notepad.exe")

    -- 如果没找到，启动它
    if pid == 0 then
        print("记事本未运行，正在启动...")
        pid = startProcess("notepad.exe")
        if pid == 0 then
            print("启动记事本失败")
            return
        end
        wingman.util.sleep(1000)
    end

    -- 确认进程存在
    if processExists(pid) then
        print(string.format("记事本正在运行 (PID: %d)", pid))

        -- 等待 3 秒后关闭
        print("3 秒后将关闭记事本...")
        wingman.util.sleep(3000)

        terminateProcess(pid)
    end
end

-- 示例：游戏进程监控
local function gameMonitorExample()
    print("\n--- 游戏进程监控示例 ---")

    local gameProcess = "explorer.exe"  -- 示例用 explorer 代替游戏

    print(string.format("监控进程: %s", gameProcess))
    print("按 Ctrl+C 停止监控")

    while true do
        local pid = wingman.process.find(gameProcess)
        if pid ~= 0 then
            local path = wingman.process.getPath(pid)
            print(string.format("[%s] %s (PID: %d)",
                os.date("%H:%M:%S"), gameProcess, pid))
        else
            print(string.format("[%s] %s 未运行",
                os.date("%H:%M:%S"), gameProcess))
        end

        wingman.util.sleep(5000)  -- 每 5 秒检查一次
    end
end

-- 运行示例
findProcess("chrome.exe")
notepadExample()

-- 取消注释以运行游戏监控示例
-- gameMonitorExample()

print("=== 完成 ===")
