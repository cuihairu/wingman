#include "wingman/vision.hpp"
#include <spdlog/spdlog.h>

// OpenCV Vision stub implementation
// When OpenCV is not available, provide stub functions

namespace wingman {

// ========== Color Detection ==========

std::optional<Point> Vision::findColor(const Color& color, const Rect& region) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return std::nullopt;
}

std::optional<Point> Vision::findColor(const Color& color, int tolerance, const Rect& region) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return std::nullopt;
}

std::vector<Point> Vision::findAllColors(const Color& color, int tolerance, const Rect& region) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {};
}

bool Vision::hasColor(const Color& color, int tolerance, const Rect& region) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return false;
}

Color Vision::getDominantColor(const Rect& region) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return Color();
}

// ========== Image Matching ==========

ImageMatch Vision::findImage(const std::string& templatePath, double threshold) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {false, Point(), 0.0, Rect()};
}

ImageMatch Vision::findImage(const std::string& templatePath, const Rect& searchRegion, double threshold) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {false, Point(), 0.0, Rect()};
}

std::vector<ImageMatch> Vision::findAllImages(const std::string& templatePath, double threshold) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {};
}

bool Vision::waitForImage(const std::string& templatePath, int timeoutMs, double threshold) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return false;
}

// ========== Shape Detection ==========

std::vector<Point> Vision::detectEdges(const Rect& region, int threshold1, int threshold2) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {};
}

std::vector<std::vector<Point>> Vision::detectContours(const Rect& region) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {};
}

std::vector<std::pair<Point, int>> Vision::detectCircles(const Rect& region, int minRadius, int maxRadius) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return {};
}

// ========== Image Processing ==========

bool Vision::captureRegion(const Rect& region, const std::string& outputPath) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return false;
}

bool Vision::saveImage(const std::string& path, const Bitmap& bitmap) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return false;
}

double Vision::compareImages(const std::string& path1, const std::string& path2) {
    spdlog::warn("Vision support not enabled (OpenCV not available)");
    return 0.0;
}

bool Vision::isColorMatch(const Color& c1, const Color& c2, int tolerance) {
    int dr = abs((int)c1.r - (int)c2.r);
    int dg = abs((int)c1.g - (int)c2.g);
    int db = abs((int)c1.b - (int)c2.b);
    return dr <= tolerance && dg <= tolerance && db <= tolerance;
}

} // namespace wingman
