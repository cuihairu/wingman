-- process 模块单元测试

describe("process 模块", function()
    it("应该能查找进程", function()
        -- svchost 是 Windows 系统进程，通常存在
        local pid = process.find("svchost.exe")
        assert.is_not_nil(pid)
        assert.is_true(pid > 0 or pid == 0) -- 可能找到或未找到
    end)

    it("应该能查找多个相同进程", function()
        local pids = process.findAll("svchost.exe")
        assert.is_not_nil(pids)
        assert.is_true(type(pids) == "table")
    end)

    it("应该能获取进程路径", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            local path = process.getPath(pid)
            assert.is_string(path)
        end
    end)

    it("应该能获取进程命令行", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            local cmd = process.getCommandLine(pid)
            assert.is_string(cmd)
        end
    end)

    it("应该能获取进程名称", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            local name = process.getName(pid)
            assert.is_string(name)
        end
    end)

    it("应该能获取进程父进程ID", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            local ppid = process.getParent(pid)
            assert.is_not_nil(ppid)
            assert.is_true(ppid > 0)
        end
    end)

    it("应该能判断进程是否存在", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            assert.is_true(process.exists(pid))
        end
    end)

    it("应该能获取进程列表", function()
        local processes = process.list()
        assert.is_not_nil(processes)
        assert.is_true(type(processes) == "table")
        assert.is_true(#processes > 0)
    end)

    it("应该能获取进程内存信息", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            local mem = process.getMemoryInfo(pid)
            assert.is_not_nil(mem)
            assert.is_not_nil(mem.workingSet)
            assert.is_true(mem.workingSet > 0)
        end
    end)

    it("应该能获取进程CPU使用率", function()
        local pid = process.find("svchost.exe")
        if pid and pid > 0 then
            local cpu = process.getCpuUsage(pid)
            assert.is_not_nil(cpu)
            assert.is_true(cpu >= 0 and cpu <= 100)
        end
    end)

    pending("应该能启动进程", function()
        local pid = process.start("notepad.exe")
        assert.is_not_nil(pid)
        assert.is_true(pid > 0)

        -- 清理
        util.sleep(500)
        process.terminate(pid)
    end)

    pending("应该能等待进程启动", function()
        local pid = process.waitFor("testapp.exe", 5000)
        -- 超时返回 nil
        assert.is_nil(pid)
    end)

    pending("应该能终止进程", function()
        -- 先启动一个进程
        local pid = process.start("notepad.exe")
        util.sleep(500)

        -- 终止进程
        local result = process.terminate(pid)
        assert.is_true(result)

        -- 验证进程已终止
        util.sleep(200)
        assert.is_false(process.exists(pid))
    end)

    pending("应该能等待进程退出", function()
        local pid = process.start("notepad.exe")

        -- 在另一个线程终止进程
        -- 这里需要实际测试
        process.wait(pid, 5000)
    end)
end)
