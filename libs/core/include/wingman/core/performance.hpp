#pragma once

#include "wingman/screen.hpp"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <opencv2/opencv.hpp>

namespace wingman {

/**
 * 图像缓存项
 */
struct CachedImage {
    cv::Mat mat;
    std::string path;
    size_t accessCount;
    uint64_t lastAccess;

    CachedImage() : accessCount(0), lastAccess(0) {}
};

/**
 * 性能配置
 */
struct PerformanceConfig {
    bool enableImageCache = true;          // 启用图像缓存
    size_t maxCacheSize = 50;              // 最大缓存图像数量
    bool enableParallelProcessing = true;  // 启用并行处理
    int numThreads = 0;                    // 线程数 (0 = 自动)

    // 查找优化
    bool useColorReduction = false;        // 使用颜色降级加速
    int colorReductionBits = 5;            // 降级位数

    // 图像匹配优化
    bool useImagePyramids = true;          // 使用图像金字塔加速
    int minPyramidLevel = 2;               // 最小金字塔层级
};

/**
 * 性能优化管理器
 *
 * 提供以下优化功能:
 * 1. 图像缓存 - 缓存已加载的模板图像，避免重复加载
 * 2. 并行处理 - 使用多线程加速像素检测
 * 3. 图像金字塔 - 加速大图像匹配
 */
class PerformanceManager {
public:
    static PerformanceManager& instance();

    // 设置配置
    void setConfig(const PerformanceConfig& config);
    const PerformanceConfig& getConfig() const;

    // ========== 图像缓存 ==========

    /**
     * 获取缓存的图像
     * 如果缓存中没有，则从文件加载并缓存
     */
    cv::Mat getCachedImage(const std::string& path);

    /**
     * 预加载图像到缓存
     */
    void preloadImage(const std::string& path);

    /**
     * 清除所有缓存
     */
    void clearCache();

    /**
     * 清除过期缓存 (LRU)
     */
    void evictExpired();

    /**
     * 获取缓存统计
     */
    size_t getCacheSize() const;
    size_t getCacheHits() const;
    size_t getCacheMisses() const;

    // ========== 优化的图像匹配 ==========

    /**
     * 快速图像匹配（使用缓存和金字塔）
     */
    bool fastFindImage(const std::string& imagePath, const Rect& region,
                       double threshold, Point& result);

    /**
     * 批量图像匹配
     */
    std::vector<std::pair<std::string, Point>> findMultipleImages(
        const std::vector<std::string>& imagePaths,
        const Rect& region,
        double threshold
    );

    // ========== 并行像素检测 ==========

    /**
     * 并行查找颜色点
     */
    std::vector<Point> parallelFindColors(
        const Color& color,
        const Rect& region,
        int tolerance,
        int maxCount = 0
    );

    /**
     * 并行查找多个颜色
     */
    std::vector<std::pair<Color, std::vector<Point>>> parallelFindMultipleColors(
        const std::vector<Color>& colors,
        const Rect& region,
        int tolerance,
        int maxCountPerColor = 10
    );

    // ========== 性能统计 ==========

    struct Stats {
        uint64_t totalCaptures = 0;
        uint64_t totalColorSearches = 0;
        uint64_t totalImageSearches = 0;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        double avgCaptureTime = 0;
        double avgColorSearchTime = 0;
        double avgImageSearchTime = 0;
    };

    Stats getStats() const;
    void resetStats();

private:
    PerformanceManager();
    ~PerformanceManager() = default;

    PerformanceManager(const PerformanceManager&) = delete;
    PerformanceManager& operator=(const PerformanceManager&) = delete;

    mutable std::mutex m_mutex;
    PerformanceConfig m_config;
    std::unordered_map<std::string, CachedImage> m_imageCache;
    size_t m_cacheHits = 0;
    size_t m_cacheMisses = 0;
    Stats m_stats;

    // ========== 辅助方法 ==========

    /**
     * 创建图像金字塔
     */
    std::vector<cv::Mat> buildPyramid(const cv::Mat& image, int maxLevel);

    /**
     * 使用金字塔进行模板匹配
     */
    bool pyramidMatch(const cv::Mat& screen, const cv::Mat& templ,
                     double threshold, cv::Point& result);

    /**
     * 颜色降级（加速）
     */
    void reduceColors(cv::Mat& image, int bits);
};

/**
 * 性能计数器
 */
class PerformanceTimer {
public:
    PerformanceTimer(double* target)
        : m_target(target), m_start(cv::getTickCount()) {}

    ~PerformanceTimer() {
        if (m_target) {
            double elapsed = (cv::getTickCount() - m_start) / cv::getTickFrequency() * 1000;
            *m_target = (*m_target * 0.9 + elapsed * 0.1); // 移动平均
        }
    }

private:
    double* m_target;
    int64 m_start;
};

} // namespace wingman
