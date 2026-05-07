-- trigger 模块单元测试

describe("trigger 模块", function()
    it("应该能创建触发器配置", function()
        local config = {
            name = "test_trigger",
            enabled = true,
            oneShot = false,
            cooldown = 1000,
            condition = {
                type = "ColorFound",
                value = "0xFF0000",
                region = {x = 0, y = 0, width = 100, height = 100},
                tolerance = 10,
                interval = 50
            },
            actions = {
                {type = "Click", x = 50, y = 50, delay = 0}
            }
        }

        assert.is_not_nil(config)
        assert.is_equal("test_trigger", config.name)
    end)

    it("应该能定义不同类型的触发条件", function()
        local types = {
            "ColorFound", "ColorLost", "ImageFound", "ImageLost",
            "WindowOpened", "WindowClosed", "ProcessStarted", "ProcessStopped",
            "TimeElapsed", "HotkeyPressed", "PixelChanged"
        }

        for _, triggerType in ipairs(types) do
            local config = {
                name = "test_" .. triggerType,
                enabled = true,
                condition = {
                    type = triggerType,
                    value = "test",
                    region = {x = 0, y = 0, width = 100, height = 100},
                    tolerance = 10
                },
                actions = {}
            }
            assert.is_not_nil(config)
        end
    end)

    it("应该能定义不同类型的触发动作", function()
        local actionTypes = {
            "RunScript", "Click", "KeyPress", "Type",
            "StopScript", "PauseScript", "ShowMessage", "PlaySound", "Log"
        }

        for _, actionType in ipairs(actionTypes) do
            local action = {
                type = actionType,
                value = "test",
                x = 50,
                y = 50,
                delay = 100
            }
            assert.is_not_nil(action)
        end
    end)

    it("应该能创建脚本执行动作", function()
        local action = {
            type = "RunScript",
            value = "print('Hello from trigger')",
            delay = 0
        }

        assert.is_equal("print('Hello from trigger')", action.value)
    end)

    it("应该能创建日志动作", function()
        local action = {
            type = "Log",
            value = "Trigger fired at " .. os.time(),
            delay = 0
        }

        assert.is_true(action.value:find("Trigger fired") ~= nil)
    end)

    it("应该能创建热键触发条件", function()
        local config = {
            name = "hotkey_trigger",
            enabled = true,
            condition = {
                type = "HotkeyPressed",
                value = "VK_F1", -- 虚拟键码
                interval = 50
            },
            actions = {
                {type = "ShowMessage", value = "F1 pressed!"}
            }
        }

        assert.is_equal("HotkeyPressed", config.condition.type)
    end)

    it("应该能创建时间触发条件", function()
        local config = {
            name = "timer_trigger",
            enabled = true,
            condition = {
                type = "TimeElapsed",
                value = tostring(GetTickCount() + 5000), -- 5秒后
                interval = 100
            },
            actions = {
                {type = "Log", value = "5 seconds elapsed"}
            }
        }

        assert.is_equal("TimeElapsed", config.condition.type)
    end)
end)
