#include "wingman/performance.hpp"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace wingman {

// ============================================================================
// PerformanceManager Implementation
// ============================================================================

int PerformanceManager::getNumCpuCores() {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return static_cast<int>(sysInfo.dwNumberOfProcessors);
#elif defined(_SC_NPROCESSORS_ONLN)
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return static_cast<int>(std::thread::hardware_concurrency());
#endif
}

PerformanceManager& PerformanceManager::instance() {
    static PerformanceManager inst;
    return inst;
}

PerformanceManager::PerformanceManager() {
    m_config.numThreads = getNumCpuCores();

    if (m_config.enableParallelProcessing) {
        cv::setNumThreads(m_config.numThreads);
    }
}

void PerformanceManager::setConfig(const PerformanceConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;

    if (m_config.enableParallelProcessing) {
        cv::setNumThreads(m_config.numThreads > 0 ? m_config.numThreads : cv::getNumThreads());
    }
}

const PerformanceConfig& PerformanceManager::getConfig() const {
    return m_config;
}

// ========== Image Cache ==========

cv::Mat PerformanceManager::getCachedImage(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_config.enableImageCache) {
        cv::Mat mat = cv::imread(path, cv::IMREAD_COLOR);
        m_cacheMisses++;
        return mat;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    auto it = m_imageCache.find(path);
    if (it != m_imageCache.end()) {
        it->second.accessCount++;
        it->second.lastAccess = now;
        m_cacheHits++;
        return it->second.mat.clone();
    }

    cv::Mat mat = cv::imread(path, cv::IMREAD_COLOR);
    if (mat.empty()) {
        m_cacheMisses++;
        return cv::Mat();
    }

    if (m_imageCache.size() >= m_config.maxCacheSize) {
        evictExpired();
    }

    CachedImage cached;
    cached.mat = mat;
    cached.path = path;
    cached.accessCount = 1;
    cached.lastAccess = now;

    m_imageCache[path] = cached;
    m_cacheMisses++;

    return mat.clone();
}

void PerformanceManager::preloadImage(const std::string& path) {
    getCachedImage(path);
}

void PerformanceManager::clearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_imageCache.clear();
}

void PerformanceManager::evictExpired() {
    if (m_imageCache.empty()) return;

    auto minIt = std::min_element(
        m_imageCache.begin(), m_imageCache.end(),
        [](const auto& a, const auto& b) {
            if (a.second.accessCount != b.second.accessCount) {
                return a.second.accessCount < b.second.accessCount;
            }
            return a.second.lastAccess < b.second.lastAccess;
        }
    );

    if (minIt != m_imageCache.end()) {
        m_imageCache.erase(minIt);
    }
}

size_t PerformanceManager::getCacheSize() const {
    return m_imageCache.size();
}

size_t PerformanceManager::getCacheHits() const {
    return m_cacheHits;
}

size_t PerformanceManager::getCacheMisses() const {
    return m_cacheMisses;
}

// ========== Optimized Image Matching ==========

bool PerformanceManager::fastFindImage(const std::string& imagePath,
                                       const Rect& region,
                                       double threshold, Point& result) {
    PerformanceTimer timer(&m_stats.avgImageSearchTime);
    m_stats.totalImageSearches++;

    cv::Mat templ = getCachedImage(imagePath);
    if (templ.empty()) {
        return false;
    }

    auto screenBitmap = Screen::capture(region);
    if (!screenBitmap) {
        return false;
    }

    int width = screenBitmap->getWidth();
    int height = screenBitmap->getHeight();
    cv::Mat screenMat(height, width, CV_8UC4, screenBitmap->getData());

    cv::Mat screenBGR;
    cv::cvtColor(screenMat, screenBGR, cv::COLOR_BGRA2BGR);

    if (templ.rows > screenBGR.rows || templ.cols > screenBGR.cols) {
        return false;
    }

    if (m_config.useImagePyramids && templ.cols > 50 && templ.rows > 50) {
        cv::Point matchPos;
        if (pyramidMatch(screenBGR, templ, threshold, matchPos)) {
            result.x = region.x + matchPos.x;
            result.y = region.y + matchPos.y;
            return true;
        }
        return false;
    }

    cv::Mat matchResult;
    cv::matchTemplate(screenBGR, templ, matchResult, cv::TM_CCOEFF_NORMED);

    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(matchResult, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        result.x = region.x + maxLoc.x;
        result.y = region.y + maxLoc.y;
        return true;
    }

    return false;
}

std::vector<std::pair<std::string, Point>> PerformanceManager::findMultipleImages(
    const std::vector<std::string>& imagePaths,
    const Rect& region,
    double threshold
) {
    std::vector<std::pair<std::string, Point>> results;

    auto screenBitmap = Screen::capture(region);
    if (!screenBitmap) {
        return results;
    }

    int width = screenBitmap->getWidth();
    int height = screenBitmap->getHeight();
    cv::Mat screenMat(height, width, CV_8UC4, screenBitmap->getData());
    cv::Mat screenBGR;
    cv::cvtColor(screenMat, screenBGR, cv::COLOR_BGRA2BGR);

    for (const auto& path : imagePaths) {
        cv::Mat templ = getCachedImage(path);
        if (templ.empty()) continue;

        if (templ.rows > screenBGR.rows || templ.cols > screenBGR.cols) {
            continue;
        }

        cv::Mat matchResult;
        cv::matchTemplate(screenBGR, templ, matchResult, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(matchResult, nullptr, &maxVal, nullptr, &maxLoc);

        if (maxVal >= threshold) {
            results.emplace_back(path, Point(region.x + maxLoc.x, region.y + maxLoc.y));
        }
    }

    return results;
}

// ========== Parallel Pixel Detection ==========

std::vector<Point> PerformanceManager::parallelFindColors(
    const Color& color,
    const Rect& region,
    int tolerance,
    int maxCount
) {
    PerformanceTimer timer(&m_stats.avgColorSearchTime);
    m_stats.totalColorSearches++;

    std::vector<Point> results;
    auto bitmap = Screen::capture(region);
    if (!bitmap) {
        return results;
    }

    int width = bitmap->getWidth();
    int height = bitmap->getHeight();
    const uint8_t* data = bitmap->getData();

    int rMin = std::max(0, (int)color.r - tolerance);
    int rMax = std::min(255, (int)color.r + tolerance);
    int gMin = std::max(0, (int)color.g - tolerance);
    int gMax = std::min(255, (int)color.g + tolerance);
    int bMin = std::max(0, (int)color.b - tolerance);
    int bMax = std::min(255, (int)color.b + tolerance);

    cv::Mat mat(height, width, CV_8UC4, (void*)data);

    cv::Mat mask(height, width, CV_8UC1);
    cv::parallel_for_(
        cv::Range(0, height),
        [&](const cv::Range& range) {
            for (int y = range.start; y < range.end; ++y) {
                const uint8_t* row = mat.ptr<uint8_t>(y);
                uint8_t* maskRow = mask.ptr<uint8_t>(y);

                for (int x = 0; x < width; ++x) {
                    const uint8_t* pixel = row + x * 4;
                    uint8_t b = pixel[0];
                    uint8_t g = pixel[1];
                    uint8_t r = pixel[2];

                    if (r >= rMin && r <= rMax &&
                        g >= gMin && g <= gMax &&
                        b >= bMin && b <= bMax) {
                        maskRow[x] = 255;
                    } else {
                        maskRow[x] = 0;
                    }
                }
            }
        },
        m_config.numThreads > 0 ? m_config.numThreads : -1
    );

    for (int y = 0; y < height; ++y) {
        const uint8_t* maskRow = mask.ptr<uint8_t>(y);
        for (int x = 0; x < width; ++x) {
            if (maskRow[x]) {
                results.emplace_back(region.x + x, region.y + y);
                if (maxCount > 0 && results.size() >= static_cast<size_t>(maxCount)) {
                    return results;
                }
            }
        }
    }

    return results;
}

std::vector<std::pair<Color, std::vector<Point>>> PerformanceManager::parallelFindMultipleColors(
    const std::vector<Color>& colors,
    const Rect& region,
    int tolerance,
    int maxCountPerColor
) {
    std::vector<std::pair<Color, std::vector<Point>>> results;

    for (const auto& color : colors) {
        auto points = parallelFindColors(color, region, tolerance, maxCountPerColor);
        results.emplace_back(color, points);
    }

    return results;
}

// ========== Performance Statistics ==========

PerformanceManager::Stats PerformanceManager::getStats() const {
    return m_stats;
}

void PerformanceManager::resetStats() {
    m_stats = Stats();
    m_cacheHits = 0;
    m_cacheMisses = 0;
}

// ========== Helper Methods ==========

std::vector<cv::Mat> PerformanceManager::buildPyramid(const cv::Mat& image, int maxLevel) {
    std::vector<cv::Mat> pyramid;
    pyramid.push_back(image);

    cv::Mat current = image;
    for (int i = 1; i < maxLevel; ++i) {
        if (current.cols < 32 || current.rows < 32) break;
        cv::Mat downsampled;
        cv::pyrDown(current, downsampled);
        pyramid.push_back(downsampled);
        current = downsampled;
    }

    return pyramid;
}

bool PerformanceManager::pyramidMatch(const cv::Mat& screen, const cv::Mat& templ,
                                      double threshold, cv::Point& result) {
    auto screenPyramid = buildPyramid(screen, m_config.minPyramidLevel);
    auto templPyramid = buildPyramid(templ, m_config.minPyramidLevel);

    cv::Point predictedPos(0, 0);

    for (int level = static_cast<int>(screenPyramid.size()) - 1; level >= 0; --level) {
        const cv::Mat& screenLevel = screenPyramid[level];
        const cv::Mat& templLevel = templPyramid[level];

        cv::Mat searchRegion = screenLevel;
        int offsetX = 0, offsetY = 0;

        if (level < static_cast<int>(screenPyramid.size()) - 1) {
            int scale = 1 << (screenPyramid.size() - 1 - level);
            int searchSize = 50 * scale;
            int searchX = std::max(0, predictedPos.x * scale - searchSize / 2);
            int searchY = std::max(0, predictedPos.y * scale - searchSize / 2);
            int searchW = std::min(screenLevel.cols - searchX, searchSize);
            int searchH = std::min(screenLevel.rows - searchY, searchSize);

            if (searchW > templLevel.cols && searchH > templLevel.rows) {
                searchRegion = screenLevel(cv::Rect(searchX, searchY, searchW, searchH));
                offsetX = searchX;
                offsetY = searchY;
            }
        }

        cv::Mat matchResult;
        cv::matchTemplate(searchRegion, templLevel, matchResult, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(matchResult, nullptr, &maxVal, nullptr, &maxLoc);

        if (maxVal < threshold) {
            return false;
        }

        predictedPos.x = maxLoc.x + offsetX;
        predictedPos.y = maxLoc.y + offsetY;

        if (level == 0) {
            result = predictedPos;
            return true;
        }
    }

    return true;
}

void PerformanceManager::reduceColors(cv::Mat& image, int bits) {
    if (bits < 1 || bits > 8) return;

    int shift = 8 - bits;
    uint8_t mask = 0xFF << shift;

    cv::Mat maskMat(image.size(), image.type(), cv::Scalar(mask, mask, mask, 0));
    cv::bitwise_and(image, maskMat, image);

    image.convertTo(image, -1, 1.0, 0);
}

} // namespace wingman
