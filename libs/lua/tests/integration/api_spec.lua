-- Wingman 集成测试示例

describe("屏幕和输入集成测试", function()
    it("应该能完整执行屏幕截图流程", function()
        -- 获取屏幕尺寸
        local width = screen.getScreenWidth()
        local height = screen.getScreenHeight()

        assert.is_true(width > 0)
        assert.is_true(height > 0)

        -- 截图（如果有实现）
        -- local bitmap = screen.capture()
        -- assert.is_not_nil(bitmap)
    end)

    it("应该能移动鼠标并获取位置", function()
        local original = input.getMousePosition()

        -- 移动到 (100, 100)
        input.move(100, 100)
        util.sleep(100)  -- 等待移动完成

        local pos = input.getMousePosition()
        -- 允许一些误差
        assert.is_true(math.abs(pos.x - 100) < 10)
        assert.is_true(math.abs(pos.y - 100) < 10)

        -- 移回原位置
        input.move(original.x, original.y)
    end)
end)

describe("窗口管理集成测试", function()
    it("应该能获取并操作前台窗口", function()
        local hwnd = window.getForeground()
        assert.is_not_nil(hwnd)

        local title = window.getTitle(hwnd)
        assert.is_string(title)

        local bounds = window.getBounds(hwnd)
        assert.is_not_nil(bounds)
        assert.is_true(bounds.width > 0)
        assert.is_true(bounds.height > 0)
    end)
end)

describe("自动化流程集成测试", function()
    it("应该能执行简单的自动化任务", function()
        -- 获取系统信息
        local cpu = system.getCpuInfo()
        local mem = system.getMemoryInfo()

        assert.is_not_nil(cpu)
        assert.is_not_nil(mem)

        -- 使用 KV 存储缓存信息
        local cacheKey = "system_cache_" .. os.time()
        kv.set(cacheKey, json.encode({
            cpuCores = cpu.cores,
            memTotal = mem.total,
            timestamp = util.getTime()
        }))

        local cached = kv.get(cacheKey)
        assert.is_not_nil(cached)

        local data = json.decode(cached)
        assert.is_equal(cpu.cores, data.cpuCores)

        -- 清理
        kv.del(cacheKey)
    end)
end)

describe("HTTP 通信集成测试", function()
    pending("与本地服务器通信", function()
        -- 需要先启动本地服务器
        local resp = http.post("http://localhost:8080/api/test", json.encode({
            action = "ping"
        }))

        assert.is_not_nil(resp)
        assert.is_equal(200, resp.status)
    end)
end)
