-- script 模块单元测试

describe("script 模块", function()
    it("应该能获取脚本列表", function()
        local scripts = script.list()
        assert.is_not_nil(scripts)
        assert.is_true(type(scripts) == "table")
    end)

    it("应该能获取脚本状态", function()
        local scripts = script.list()
        if #scripts > 0 then
            local state = script.getState(scripts[1])
            assert.is_not_nil(state)
            assert.is_string(state)
        end
    end)

    it("应该能判断脚本是否运行", function()
        local scripts = script.list()
        if #scripts > 0 then
            local running = script.isRunning(scripts[1])
            assert.is_boolean(running)
        end
    end)

    it("应该能获取脚本配置", function()
        local scripts = script.list()
        if #scripts > 0 then
            local config = script.getConfig(scripts[1])
            assert.is_not_nil(config)
        end
    end)

    it("应该能设置脚本配置", function()
        local testScript = "test_script"
        script.setConfig(testScript, {autoReload = true})
        local config = script.getConfig(testScript)
        assert.is_not_nil(config)
    end)

    it("应该能设置环境变量", function()
        script.setEnv("TEST_VAR", "test_value")
        local value = script.getEnv("TEST_VAR")
        assert.is_equal("test_value", value)
    end)

    it("应该能获取不存在的环境变量", function()
        local value = script.getEnv("NONEXISTENT_VAR")
        assert.is_equal("", value)
    end)

    it("应该能设置热加载", function()
        local testScript = "test_script"
        script.setAutoReload(testScript, true)
        assert.is_true(true) -- 不应报错
    end)

    it("应该能启用全局热加载", function()
        script.setGlobalAutoReload(true)
        assert.is_true(true) -- 不应报错
    end)

    pending("应该能加载脚本", function()
        local result = script.load("test", "scripts/examples/hello.lua")
        assert.is_true(result)
    end)

    pending("应该能卸载脚本", function()
        script.load("test", "scripts/examples/hello.lua")
        local result = script.unload("test")
        assert.is_true(result)
    end)

    pending("应该能重新加载脚本", function()
        script.load("test", "scripts/examples/hello.lua")
        local result = script.reload("test")
        assert.is_true(result)
    end)

    pending("应该能运行脚本", function()
        script.load("test", "scripts/examples/hello.lua")
        local result = script.run("test")
        assert.is_true(result)
    end)

    pending("应该能停止脚本", function()
        script.load("test", "scripts/examples/hello.lua")
        script.run("test")
        local result = script.stop("test")
        assert.is_true(result)
    end)

    pending("应该能暂停脚本", function()
        script.load("test", "scripts/examples/hello.lua")
        script.run("test")
        local result = script.pause("test")
        assert.is_true(result)
    end)

    pending("应该能恢复脚本", function()
        script.load("test", "scripts/examples/hello.lua")
        script.run("test")
        script.pause("test")
        local result = script.resume("test")
        assert.is_true(result)
    end)
end)
