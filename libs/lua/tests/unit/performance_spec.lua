-- performance 模块单元测试

describe("performance 模块", function()
    it("应该能获取性能配置", function()
        local config = performance.getConfig()

        assert.is_not_nil(config)
        assert.is_boolean(config.enableImageCache)
        assert.is_number(config.maxCacheSize)
        assert.is_boolean(config.enableParallelProcessing)
    end)

    it("应该能设置性能配置", function()
        local original = performance.getConfig()

        local newConfig = {
            enableImageCache = true,
            maxCacheSize = 100,
            enableParallelProcessing = true,
            numThreads = 4
        }

        performance.setConfig(newConfig)
        local current = performance.getConfig()

        assert.is_equal(newConfig.maxCacheSize, current.maxCacheSize)

        -- 恢复原始配置
        performance.setConfig(original)
    end)

    it("应该能获取缓存大小", function()
        local size = performance.getCacheSize()
        assert.is_number(size)
        assert.is_true(size >= 0)
    end)

    it("应该能清除缓存", function()
        performance.clearCache()
        local size = performance.getCacheSize()
        assert.is_equal(0, size)
    end)

    it("应该能预加载图像", function()
        -- 测试预加载不存在的图像（不应报错）
        performance.preloadImage("nonexistent.png")
        assert.is_true(true)
    end)

    it("应该能获取缓存统计", function()
        local stats = performance.getStats()

        assert.is_not_nil(stats)
        assert.is_number(stats.totalCaptures)
        assert.is_number(stats.totalColorSearches)
        assert.is_number(stats.totalImageSearches)
        assert.is_number(stats.cacheHits)
        assert.is_number(stats.cacheMisses)
    end)

    it("应该能重置统计", function()
        performance.resetStats()
        local stats = performance.getStats()

        assert.is_equal(0, stats.totalCaptures)
        assert.is_equal(0, stats.totalColorSearches)
        assert.is_equal(0, stats.totalImageSearches)
    end)

    it("应该能获取平均捕获时间", function()
        local stats = performance.getStats()
        assert.is_number(stats.avgCaptureTime)
    end)

    it("应该能获取平均颜色搜索时间", function()
        local stats = performance.getStats()
        assert.is_number(stats.avgColorSearchTime)
    end)

    it("应该能获取平均图像搜索时间", function()
        local stats = performance.getStats()
        assert.is_number(stats.avgImageSearchTime)
    end)

    it("应该能执行并行颜色查找", function()
        local results = performance.parallelFindColors(0xFF0000, {x=0, y=0, width=1920, height=1080}, 10, 100)

        assert.is_not_nil(results)
        assert.is_true(type(results) == "table")
    end)

    it("应该能执行批量图像查找", function()
        local imagePaths = {"test1.png", "test2.png"}
        local results = performance.findMultipleImages(imagePaths, {x=0, y=0, width=1920, height=1080}, 0.8)

        assert.is_not_nil(results)
        assert.is_true(type(results) == "table")
    end)

    it("应该能查找多个颜色", function()
        local colors = {
            {r=255, g=0, b=0},
            {r=0, g=255, b=0},
            {r=0, g=0, b=255}
        }

        local results = performance.parallelFindMultipleColors(colors, {x=0, y=0, width=100, height=100}, 10, 5)

        assert.is_not_nil(results)
        assert.is_true(type(results) == "table")
        assert.is_equal(#colors, #results)
    end)
end)
