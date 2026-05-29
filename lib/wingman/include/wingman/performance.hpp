#pragma once

#include "wingman/screen.hpp"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include <opencv2/opencv.hpp>

namespace wingman {

/**
 * Image cache entry
 */
struct CachedImage {
    cv::Mat mat;
    std::string path;
    size_t accessCount;
    uint64_t lastAccess;

    CachedImage() : accessCount(0), lastAccess(0) {}
};

/**
 * Performance configuration
 */
struct PerformanceConfig {
    bool enableImageCache = true;          // Enable image cache
    size_t maxCacheSize = 50;              // Max cached image count
    bool enableParallelProcessing = true;  // Enable parallel processing
    int numThreads = 0;                    // Thread count (0 = auto)

    // Search optimization
    bool useColorReduction = false;        // Use color reduction for acceleration
    int colorReductionBits = 5;            // Reduction bits

    // Image matching optimization
    bool useImagePyramids = true;          // Use image pyramid for acceleration
    int minPyramidLevel = 2;               // Minimum pyramid level
};

/**
 * Performance optimization manager
 *
 * Provides the following optimization features:
 * 1. Image cache - caches loaded template images to avoid repeated loading
 * 2. Parallel processing - uses multi-threading to accelerate pixel detection
 * 3. Image pyramid - accelerates large image matching
 */
class PerformanceManager {
public:
    static PerformanceManager& instance();

    // Set configuration
    void setConfig(const PerformanceConfig& config);
    const PerformanceConfig& getConfig() const;

    // ========== Image cache ==========

    /**
     * Get cached image
     * If not in cache, loads from file and caches
     */
    cv::Mat getCachedImage(const std::string& path);

    /**
     * Preload image to cache
     */
    void preloadImage(const std::string& path);

    /**
     * Clear all cache
     */
    void clearCache();

    /**
     * Evict expired cache (LRU)
     */
    void evictExpired();

    /**
     * Get cache statistics
     */
    size_t getCacheSize() const;
    size_t getCacheHits() const;
    size_t getCacheMisses() const;

    // ========== Optimized image matching ==========

    /**
     * Fast image matching (using cache and pyramid)
     */
    bool fastFindImage(const std::string& imagePath, const Rect& region,
                       double threshold, Point& result);

    /**
     * Batch image matching
     */
    std::vector<std::pair<std::string, Point>> findMultipleImages(
        const std::vector<std::string>& imagePaths,
        const Rect& region,
        double threshold
    );

    // ========== Parallel pixel detection ==========

    /**
     * Parallel find color points
     */
    std::vector<Point> parallelFindColors(
        const Color& color,
        const Rect& region,
        int tolerance,
        int maxCount = 0
    );

    /**
     * Parallel find multiple colors
     */
    std::vector<std::pair<Color, std::vector<Point>>> parallelFindMultipleColors(
        const std::vector<Color>& colors,
        const Rect& region,
        int tolerance,
        int maxCountPerColor = 10
    );

    // ========== Performance statistics ==========

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

    // ========== Helper methods ==========

    /**
     * Build image pyramid
     */
    std::vector<cv::Mat> buildPyramid(const cv::Mat& image, int maxLevel);

    /**
     * Template matching using pyramid
     */
    bool pyramidMatch(const cv::Mat& screen, const cv::Mat& templ,
                     double threshold, cv::Point& result);

    /**
     * Color reduction (acceleration)
     */
    void reduceColors(cv::Mat& image, int bits);

    /**
     * Get CPU core count (cross-platform)
     */
    static int getNumCpuCores();
};

/**
 * Performance timer
 */
class PerformanceTimer {
public:
    PerformanceTimer(double* target)
        : m_target(target), m_start(cv::getTickCount()) {}

    ~PerformanceTimer() {
        if (m_target) {
            double elapsed = (cv::getTickCount() - m_start) / cv::getTickFrequency() * 1000;
            *m_target = (*m_target * 0.9 + elapsed * 0.1); // Moving average
        }
    }

private:
    double* m_target;
    int64_t m_start;
};

} // namespace wingman
