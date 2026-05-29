#pragma once

#include <string>
#include <vector>
#include <optional>
#include "wingman/screen.hpp"
#include "wingman/trigger.hpp"

namespace wingman {

// Color match result
struct ColorMatch {
    bool found;
    Point position;
    int count;  // Number of matched points
};

// Image match result
struct ImageMatch {
    bool found;
    Point position;
    double confidence;  // 0.0 - 1.0
    Rect region;        // Matched region
};

// Forward declaration for OcrResult (defined in ocr.hpp)
struct OcrResult;

// Vision module
class Vision {
public:
    // ========== Color detection ==========

    // Find single color (exact match)
    static std::optional<Point> findColor(const Color& color, const Rect& region = {});

    // Find color (with tolerance)
    static std::optional<Point> findColor(const Color& color, int tolerance, const Rect& region = {});

    // Find all matching color points
    static std::vector<Point> findAllColors(const Color& color, int tolerance = 0, const Rect& region = {});

    // Check if region contains specified color
    static bool hasColor(const Color& color, int tolerance = 0, const Rect& region = {});

    // Get dominant color in region (mode)
    static Color getDominantColor(const Rect& region = {});

    // ========== Image matching ==========

    // Find image template
    static ImageMatch findImage(const std::string& templatePath, double threshold = 0.8);

    // Find image template (specified search region)
    static ImageMatch findImage(const std::string& templatePath, const Rect& searchRegion, double threshold = 0.8);

    // Find all image matches
    static std::vector<ImageMatch> findAllImages(const std::string& templatePath, double threshold = 0.8);

    // Wait for image to appear (returns false on timeout)
    static bool waitForImage(const std::string& templatePath, int timeoutMs = 5000, double threshold = 0.8);

    // ========== Shape detection ==========

    // Detect edges
    static std::vector<Point> detectEdges(const Rect& region = {}, int threshold1 = 50, int threshold2 = 150);

    // Detect contours
    static std::vector<std::vector<Point>> detectContours(const Rect& region = {});

    // Detect circles
    static std::vector<std::pair<Point, int>> detectCircles(const Rect& region = {}, int minRadius = 10, int maxRadius = 100);

    // ========== Image processing ==========

    // Capture screen region as image
    static bool captureRegion(const Rect& region, const std::string& outputPath);

    // Save image (for debugging)
    static bool saveImage(const std::string& path, const Bitmap& bitmap);

    // Compare similarity of two images
    static double compareImages(const std::string& path1, const std::string& path2);

    // Helper: whether color is within tolerance
    static bool isColorMatch(const Color& c1, const Color& c2, int tolerance);

private:
};

} // namespace wingman
