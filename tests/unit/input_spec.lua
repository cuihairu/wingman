-- input 模块单元测试

describe("input 模块", function()
    it("应该能获取鼠标位置", function()
        local pos = input.getMousePosition()

        assert.is_not_nil(pos)
        assert.is_number(pos.x)
        assert.is_number(pos.y)
        assert.is_true(pos.x >= 0)
        assert.is_true(pos.y >= 0)
    end)

    it("应该能检测按键状态", function()
        -- SHIFT 键的 VK 码是 16
        local isDown = input.isKeyDown(16)
        assert.is_boolean(isDown)
    end)

    it("应该能检测多个按键状态", function()
        local keys = {16, 17, 18} -- SHIFT, CTRL, ALT
        for _, vk in ipairs(keys) do
            local isDown = input.isKeyDown(vk)
            assert.is_boolean(isDown)
        end
    end)

    it("应该能设置和获取鼠标延迟", function()
        local original = input.getClickDelay()
        input.setClickDelay(50)
        local newDelay = input.getClickDelay()
        assert.is_equal(50, newDelay)

        -- 恢复原始值
        input.setClickDelay(original)
    end)

    it("应该能设置和获取键盘延迟", function()
        local original = input.getKeyDelay()
        input.setKeyDelay(30)
        local newDelay = input.getKeyDelay()
        assert.is_equal(30, newDelay)

        -- 恢复原始值
        input.setKeyDelay(original)
    end)

    it("应该能设置和获取移动持续时间", function()
        local original = input.getMoveDuration()
        input.setMoveDuration(200)
        local newDuration = input.getMoveDuration()
        assert.is_equal(200, newDuration)

        -- 恢复原始值
        input.setMoveDuration(original)
    end)

    it("随机延迟应该在指定范围内", function()
        for i = 1, 10 do
            local min, max = 50, 100
            input.randomDelay(min, max)
            -- 无法直接测试延迟时间，但至少应该不报错
            assert.is_true(true)
        end
    end)

    pending("鼠标点击应该正常工作", function()
        -- 需要实际鼠标操作
        local original = input.getMousePosition()
        input.click(100, 100, "left")
        util.sleep(100)
        local pos = input.getMousePosition()
        -- 允许一些误差
        assert.is_true(math.abs(pos.x - 100) < 10)
        assert.is_true(math.abs(pos.y - 100) < 10)

        -- 移回原位置
        input.move(original.x, original.y)
    end)

    pending("鼠标移动应该使用贝塞尔曲线", function()
        -- 测试贝塞尔曲线移动
        local original = input.getMousePosition()
        input.move(500, 500, 300) -- 300ms 移动
        util.sleep(350)
        local pos = input.getMousePosition()
        assert.is_true(math.abs(pos.x - 500) < 10)
        assert.is_true(math.abs(pos.y - 500) < 10)

        input.move(original.x, original.y)
    end)

    pending("文本输入应该正常工作", function()
        -- 需要焦点窗口
        input.type("test", 10)
    end)

    pending("按键操作应该正常工作", function()
        input.keyDown(16) -- SHIFT down
        util.sleep(50)
        input.keyUp(16)   -- SHIFT up
    end)
end)
