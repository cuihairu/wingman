-- security 模块单元测试

describe("security 模块", function()
    it("应该能获取安全配置", function()
        local config = security.getConfig()

        assert.is_not_nil(config)
        assert.is_boolean(config.enableAntiDetection)
        assert.is_number(config.clickJitter)
        assert.is_number(config.moveJitter)
    end)

    it("应该能设置安全配置", function()
        local original = security.getConfig()

        local newConfig = {
            enableAntiDetection = true,
            clickJitter = 5,
            moveJitter = 10,
            randomDelay = {min = 50, max = 150}
        }

        security.setConfig(newConfig)
        local current = security.getConfig()

        assert.is_equal(newConfig.clickJitter, current.clickJitter)
        assert.is_equal(newConfig.moveJitter, current.moveJitter)

        -- 恢复原始配置
        security.setConfig(original)
    end)

    it("应该能获取随机延迟", function()
        local delay = security.getRandomDelay()
        assert.is_not_nil(delay)
        assert.is_not_nil(delay.min)
        assert.is_not_nil(delay.max)
        assert.is_true(delay.min >= 0)
        assert.is_true(delay.max >= delay.min)
    end)

    it("应该能设置随机延迟", function()
        security.setRandomDelay(100, 200)
        local delay = security.getRandomDelay()
        assert.is_equal(100, delay.min)
        assert.is_equal(200, delay.max)
    end)

    it("应该能获取随机偏移", function()
        local offset = security.getRandomOffset()
        assert.is_not_nil(offset)
        assert.is_not_nil(offset.x)
        assert.is_not_nil(offset.y)
    end)

    it("应该能获取点击抖动", function()
        local jitter = security.getClickJitter()
        assert.is_not_nil(jitter)
        assert.is_not_nil(jitter.x)
        assert.is_not_nil(jitter.y)
    end)

    it("应该能检测调试器", function()
        local isPresent = security.isDebuggerPresent()
        assert.is_boolean(isPresent)
    end)

    it("应该能检测虚拟机", function()
        local isVM = security.isRunningInVM()
        assert.is_boolean(isVM)
    end)

    it("应该能验证完整性", function()
        local valid = security.verifyIntegrity()
        assert.is_boolean(valid)
    end)

    it("应该能哈希字符串", function()
        local hash = security.hashString("test")
        assert.is_string(hash)
        assert.is_true(#hash > 0)

        -- 相同输入应产生相同哈希
        local hash2 = security.hashString("test")
        assert.is_equal(hash, hash2)

        -- 不同输入应产生不同哈希
        local hash3 = security.hashString("different")
        assert.is_not_equal(hash, hash3)
    end)

    it("应该能加密字符串", function()
        local encrypted = security.encryptString("secret")
        assert.is_string(encrypted)
        assert.is_not_equal("secret", encrypted)
    end)

    it("应该能解密字符串", function()
        local original = "secret"
        local encrypted = security.encryptString(original)
        local decrypted = security.decryptString(encrypted)

        assert.is_equal(original, decrypted)
    end)

    it("应该能生成随机字符串", function()
        local random = security.generateRandomString(16)
        assert.is_string(random)
        assert.is_equal(16, #random)

        -- 多次生成应该不同
        local random2 = security.generateRandomString(16)
        assert.is_not_equal(random, random2)
    end)

    it("应该能过滤敏感信息", function()
        local input = "password=secret123&token=abc123"
        local filtered = security.filterSensitive(input)

        assert.is_string(filtered)
        assert.is_not_equal(input, filtered)
        assert.is_true(filtered:find("password") == nil or filtered:find("secret") == nil)
    end)
end)
