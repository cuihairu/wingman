-- recorder 模块单元测试

describe("recorder 模块", function()
    it("应该能录制状态检查", function()
        -- 录制前应该未在录制
        assert.is_false(recorder.isRecording())
    end)

    pending("应该能开始录制", function()
        recorder.start()
        assert.is_true(recorder.isRecording())
    end)

    pending("应该能停止录制", function()
        recorder.start()
        util.sleep(100)
        recorder.stop()
        assert.is_false(recorder.isRecording())
    end)

    pending("应该能保存录制", function()
        recorder.start()
        util.sleep(100)
        recorder.stop()
        local result = recorder.save("test_macro")
        assert.is_true(result)
    end)

    pending("应该能播放宏", function()
        recorder.save("test_macro")
        local result = recorder.play("test_macro", 100, 1)
        assert.is_true(result)
    end)

    pending("应该能以不同速度播放宏", function()
        recorder.save("test_macro")

        -- 50% 速度
        recorder.play("test_macro", 50, 1)
        util.sleep(500)

        -- 200% 速度
        recorder.play("test_macro", 200, 1)
    end)

    pending("应该能循环播放宏", function()
        recorder.save("test_macro")
        -- 这会一直播放，需要手动停止
        -- recorder.play("test_macro", 100, 0)
    end)

    pending("应该能获取录制统计", function()
        recorder.start()
        util.sleep(100)
        recorder.stop()

        local stats = recorder.getStats()
        assert.is_not_nil(stats)
        assert.is_not_nil(stats.events)
        assert.is_not_nil(stats.duration)
    end)

    pending("应该能清除录制", function()
        recorder.start()
        util.sleep(100)
        recorder.stop()
        recorder.clear()
        local stats = recorder.getStats()
        assert.is_equal(0, stats.events)
    end)
end)
