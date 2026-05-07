-- util 模块单元测试

describe("util 模块", function()
    it("应该能休眠指定时间", function()
        local start = util.getTime()
        util.sleep(100)
        local elapsed = util.getTime() - start
        assert.is_true(elapsed >= 90) -- 允许一些误差
    end)

    it("应该能获取当前时间戳", function()
        local time = util.getTime()
        assert.is_number(time)
        assert.is_true(time > 0)
    end)

    it("应该能获取日期时间", function()
        local datetime = util.getDateTime()
        assert.is_not_nil(datetime)
        assert.is_not_nil(datetime.year)
        assert.is_not_nil(datetime.month)
        assert.is_not_nil(datetime.day)
    end)

    it("应该能格式化日期时间", function()
        local formatted = util.formatDateTime("%Y-%m-%d %H:%M:%S")
        assert.is_string(formatted)
        assert.is_true(#formatted > 0)
    end)

    it("应该能输出日志", function()
        util.log("INFO", "Test log message")
        assert.is_true(true) -- 不应报错
    end)

    it("应该能输出不同级别的日志", function()
        util.log("DEBUG", "Debug message")
        util.log("INFO", "Info message")
        util.log("WARN", "Warning message")
        util.log("ERROR", "Error message")
        assert.is_true(true)
    end)
end)
