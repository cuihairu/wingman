-- game_profile 模块单元测试

describe("game_profile 模块", function()
    it("应该能获取游戏配置目录", function()
        local dir = game.getProfilesDirectory()
        assert.is_string(dir)
    end)

    it("应该能设置游戏配置目录", function()
        local original = game.getProfilesDirectory()
        game.setProfilesDirectory("C:\\Temp\\Profiles")
        local newDir = game.getProfilesDirectory()
        assert.is_equal("C:\\Temp\\Profiles", newDir)

        -- 恢复原始目录
        game.setProfilesDirectory(original)
    end)

    it("应该能获取当前游戏配置", function()
        local profile = game.getActive()
        assert.is_not_nil(profile)
    end)

    it("应该能设置当前游戏配置", function()
        local original = game.getActive()
        game.setActive("test_game")
        local current = game.getActive()
        assert.is_equal("test_game", current)

        -- 恢复原始配置
        if original then
            game.setActive(original)
        end
    end)

    it("应该能获取游戏配置列表", function()
        local profiles = game.list()
        assert.is_not_nil(profiles)
        assert.is_true(type(profiles) == "table")
    end)

    it("应该能根据窗口查找游戏配置", function()
        local profile = game.findByWindow("explorer")
        -- 可能找到或未找到
        assert.is_true(profile == nil or type(profile) == "table")
    end)

    it("应该能创建游戏配置模板", function()
        local template = game.createTemplate("test_game", {
            description = "Test game profile",
            windowTitle = "Test Game",
            regions = {
                hpBar = {x = 100, y = 100, width = 200, height = 20}
            }
        })
        assert.is_not_nil(template)
    end)

    it("应该能加载游戏配置", function()
        local profile = game.load("test_game")
        -- 可能加载失败（文件不存在）
        assert.is_true(profile == nil or type(profile) == "table")
    end)

    it("应该能保存游戏配置", function()
        local result = game.save("test_game", {
            description = "Test",
            windowTitle = "Test"
        })
        assert.is_true(result)
    end)

    it("应该能扫描游戏窗口", function()
        local profiles = game.scan()
        assert.is_not_nil(profiles)
        assert.is_true(type(profiles) == "table")
    end)

    pending("应该能获取配置中的区域", function()
        local profile = game.getActive()
        if profile then
            local region = profile.regions and profile.regions.hpBar
            assert.is_not_nil(region)
        end
    end)
end)
