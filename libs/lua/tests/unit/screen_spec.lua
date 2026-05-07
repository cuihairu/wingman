-- Wingman 单元测试示例
-- 使用 busted 测试框架

describe("screen 模块", function()
    it("应该能获取屏幕尺寸", function()
        local width = screen.getScreenWidth()
        local height = screen.getScreenHeight()

        assert.is_number(width)
        assert.is_number(height)
        assert.is_true(width > 0)
        assert.is_true(height > 0)
    end)

    it("应该能获取像素颜色", function()
        local color = screen.getPixel(0, 0)

        assert.is_not_nil(color)
        assert.is_number(color.r)
        assert.is_number(color.g)
        assert.is_number(color.b)
    end)

    pending("查找颜色功能", function()
        -- 需要屏幕上有特定颜色
        local found, x, y = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
        assert.is_boolean(found)
    end)
end)

describe("input 模块", function()
    it("应该能获取鼠标位置", function()
        local pos = input.getMousePosition()

        assert.is_not_nil(pos)
        assert.is_number(pos.x)
        assert.is_number(pos.y)
    end)

    it("应该能检测按键状态", function()
        -- SHIFT 键的 VK 码是 16
        local isDown = input.isKeyDown(16)
        assert.is_boolean(isDown)
    end)
end)

describe("window 模块", function()
    it("应该能获取前台窗口", function()
        local hwnd = window.getForeground()
        assert.is_not_nil(hwnd)
    end)

    pending("查找窗口功能", function()
        -- 需要实际窗口存在
        local hwnd = window.find("explorer")
        -- assert.is_not_nil(hwnd)
    end)
end)

describe("process 模块", function()
    it("应该能找到当前进程", function()
        -- 在 Windows 上，每个系统都有 svchost 进程
        local pid = process.find("svchost.exe")
        assert.is_not_nil(pid)
    end)

    it("应该能获取进程列表", function()
        local pid = process.find("svchost.exe")
        if pid ~= 0 then
            local path = process.getPath(pid)
            assert.is_string(path)
        end
    end)
end)

describe("system 模块", function()
    it("应该能获取 CPU 信息", function()
        local cpu = system.getCpuInfo()

        assert.is_not_nil(cpu)
        assert.is_string(cpu.vendor)
        assert.is_string(cpu.brand)
        assert.is_number(cpu.cores)
        assert.is_true(cpu.cores > 0)
    end)

    it("应该能获取内存信息", function()
        local mem = system.getMemoryInfo()

        assert.is_not_nil(mem)
        assert.is_number(mem.total)
        assert.is_number(mem.available)
        assert.is_true(mem.total > 0)
    end)

    it("应该能获取操作系统信息", function()
        local os = system.getOsInfo()

        assert.is_not_nil(os)
        assert.is_string(os.platform)
        assert.is_equal("Windows", os.platform)
    end)
end)

describe("http 模块", function()
    pending("HTTP GET 请求", function()
        local resp = http.get("https://httpbin.org/get")

        assert.is_not_nil(resp)
        assert.is_number(resp.status)
        assert.is_equal(200, resp.status)
        assert.is_true(resp.success)
    end)

    pending("HTTP POST 请求", function()
        local resp = http.post("https://httpbin.org/post", '{"test": true}')

        assert.is_not_nil(resp)
        assert.is_equal(200, resp.status)
    end)
end)

describe("json 模块", function()
    it("应该能解析 JSON 字符串", function()
        local data = json.decode('{"name": "test", "value": 123}')

        assert.is_not_nil(data)
        assert.is_equal("test", data.name)
        assert.is_equal(123, data.value)
    end)

    it("应该能序列化为 JSON", function()
        local obj = {name = "test", value = 123}
        local str = json.encode(obj)

        assert.is_string(str)
        assert.is_true(str:find('"name"') ~= nil)
    end)
end)

describe("kv 模块", function()
    local testKey = "test_key_" .. os.time()

    it("应该能设置和获取值", function()
        kv.set(testKey, "test_value")
        local value = kv.get(testKey)

        assert.is_equal("test_value", value)
    end)

    it("应该能检查键是否存在", function()
        kv.set(testKey .. "_exists", "value")
        local exists = kv.exists(testKey .. "_exists")

        assert.is_true(exists)
    end)

    it("应该能删除键", function()
        kv.set(testKey .. "_del", "value")
        kv.del(testKey .. "_del")
        local exists = kv.exists(testKey .. "_del")

        assert.is_false(exists)
    end)

    it("应该支持 hash 操作", function()
        local hash = testKey .. "_hash"
        kv.hset(hash, "field1", "value1")
        kv.hset(hash, "field2", "value2")

        local value1 = kv.hget(hash, "field1")
        local all = kv.hgetall(hash)

        assert.is_equal("value1", value1)
        assert.is_equal("value2", all.field2)
    end)

    it("应该支持 list 操作", function()
        local list = testKey .. "_list"
        kv.lpush(list, "item1")
        kv.lpush(list, "item2")

        local len = kv.llen(list)
        local item1 = kv.lpop(list)

        assert.is_equal(2, len)
        assert.is_equal("item2", item1)
    end)

    -- 清理测试键
    teardown(function()
        kv.del(testKey)
        kv.del(testKey .. "_exists")
        kv.del(testKey .. "_del")
        kv.del(testKey .. "_hash")
        kv.del(testKey .. "_list")
    end)
end)
