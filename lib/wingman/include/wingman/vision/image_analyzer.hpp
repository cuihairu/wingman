#pragma once

#include "wingman/screen.hpp"
#include "wingman/capture/capture_source.hpp"
#include <optional>
#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace wingman::vision {

/**
 * @brief Color match result
 */
struct ColorMatch {
    Point position;        // Match position
    Color foundColor;      // Actually found color
    int distance;          // Distance to target color
    double confidence;     // Match confidence (0.0 - 1.0)
};

/**
 * @brief Image match result
 */
struct ImageMatch {
    Point position;        // Match position
    double confidence;     // Match confidence (0.0 - 1.0)
    Rect matchedRegion;    // Matched region
};

/**
 * @brief Image analyzer
 *
 * Responsibility: find colors, images, etc. from bitmaps
 */
class ImageAnalyzer {
public:
    ImageAnalyzer() = default;
    ~ImageAnalyzer() = default;

    // Non-copyable
    ImageAnalyzer(const ImageAnalyzer&) = delete;
    ImageAnalyzer& operator=(const ImageAnalyzer&) = delete;

    /**
     * @brief Find single color point
     * @param bitmap Bitmap to analyze
     * @param target Target color
     * @param region Search region
     * @param tolerance Tolerance (0-255)
     * @return Found point, returns std::nullopt if not found
     */
    std::optional<ColorMatch> findColor(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance = 0
    );

    /**
     * @brief Find all color points
     * @param bitmap Bitmap to analyze
     * @param target Target color
     * @param region Search region
     * @param tolerance Tolerance
     * @param maxCount Max return count (0 means no limit)
     * @return List of found points
     */
    std::vector<ColorMatch> findColors(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance = 0,
        int maxCount = 0
    );

    /**
     * @brief Find image
     * @param bitmap Bitmap to search
     * @param templatePath Template image path
     * @param region Search region
     * @param threshold Match threshold (0.0 - 1.0)
     * @return Match result, returns std::nullopt if not found
     */
    std::optional<ImageMatch> findImage(
        const Bitmap& bitmap,
        const std::string& templatePath,
        const Rect& region,
        double threshold = 0.8
    );

    /**
     * @brief Find image (from bitmap template)
     */
    std::optional<ImageMatch> findImage(
        const Bitmap& bitmap,
        const Bitmap& templateBitmap,
        const Rect& region,
        double threshold = 0.8
    );

    /**
     * @brief Find color from capture source (convenience method)
     * @param source Capture source
     * @param target Target color
     * @param region Search region
     * @param tolerance Tolerance
     * @return Found point
     */
    std::optional<ColorMatch> findColorFromSource(
        std::shared_ptr<capture::ICaptureSource> source,
        const Color& target,
        const Rect& region,
        int tolerance = 0
    );

    /**
     * @brief Find color asynchronously
     * @param callback Completion callback
     */
    void findColorAsync(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance,
        std::function<void(std::optional<ColorMatch>)> callback
    );

    /**
     * @brief Calculate color distance
     * @param c1 Color 1
     * @param c2 Color 2
     * @return Euclidean distance squared
     */
    static int colorDistance(const Color& c1, const Color& c2);

    /**
     * @brief Check if colors match
     * @param c1 Color 1
     * @param c2 Color 2
     * @param tolerance Tolerance
     * @return Returns true if matched
     */
    static bool colorMatches(const Color& c1, const Color& c2, int tolerance);

    /**
     * @brief Clamp region to bitmap bounds
     * @note public so PatternMatcher can use it
     */
    static Rect clampRegion(const Rect& region, const Bitmap& bitmap);

private:
    /**
     * @brief Get search region for bitmap
     */
    Rect getSearchRegion(const Bitmap& bitmap, const Rect& region);

    /**
     * @brief Check if point is in region
     */
    static bool isPointInRegion(const Point& p, const Rect& r) {
        return p.x >= r.x && p.x < r.x + r.width &&
               p.y >= r.y && p.y < r.y + r.height;
    }
};

/**
 * @brief Pattern matcher
 *
 * Provides more advanced pattern matching functionality
 */
class PatternMatcher {
public:
    /**
     * @brief Multi-color pattern matching
     * @param bitmap Bitmap to search
     * @param colors Color list (left to right, top to bottom)
     * @param offsets Relative position offsets
     * @param region Search region
     * @param tolerance Tolerance
     * @return Match result
     */
    static std::optional<Point> matchColorPattern(
        const Bitmap& bitmap,
        const std::vector<Color>& colors,
        const std::vector<Point>& offsets,
        const Rect& region,
        int tolerance = 0
    );

    /**
     * @brief Template matching (normalized cross-correlation)
     */
    static double templateMatch(
        const Bitmap& bitmap,
        const Bitmap& tpl,
        Point& bestPosition,
        const Rect& region = {}
    );

    /**
     * @brief Edge detection matching
     */
    static std::optional<Point> matchEdge(
        const Bitmap& bitmap,
        const Rect& region,
        bool horizontal = true,
        int threshold = 128
    );
};

} // namespace wingman::vision
