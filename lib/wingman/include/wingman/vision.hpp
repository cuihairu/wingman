#pragma once

#ifdef _WIN32
#include <string>
#include <vector>
#include <optional>
#include "wingman/screen.hpp"
#include "wingman/trigger.hpp"

namespace wingman {

// 颜色匹配结果
struct ColorMatch {
    bool found;
    Point position;
    int count;  // 匹配点的数量
};

// 图像匹配结果
struct ImageMatch {
    bool found;
    Point position;
    double confidence;  // 0.0 - 1.0
    Rect region;        // 匹配区域
};

// Forward declaration for OcrResult (defined in ocr.hpp)
struct OcrResult;

// Vision 视觉模块
class Vision {
public:
    // ========== 颜色检测 ==========

    // 查找单个颜色（精确匹配）
    static std::optional<Point> findColor(const Color& color, const Rect& region = {});

    // 查找颜色（带容差）
    static std::optional<Point> findColor(const Color& color, int tolerance, const Rect& region = {});

    // 查找所有匹配的颜色点
    static std::vector<Point> findAllColors(const Color& color, int tolerance = 0, const Rect& region = {});

    // 检查区域内是否包含指定颜色
    static bool hasColor(const Color& color, int tolerance = 0, const Rect& region = {});

    // 获取区域内主要颜色（众数）
    static Color getDominantColor(const Rect& region = {});

    // ========== 图像匹配 ==========

    // 查找图像模板
    static ImageMatch findImage(const std::string& templatePath, double threshold = 0.8);

    // 查找图像模板（指定搜索区域）
    static ImageMatch findImage(const std::string& templatePath, const Rect& searchRegion, double threshold = 0.8);

    // 查找所有图像匹配
    static std::vector<ImageMatch> findAllImages(const std::string& templatePath, double threshold = 0.8);

    // 等待图像出现（超时返回 false）
    static bool waitForImage(const std::string& templatePath, int timeoutMs = 5000, double threshold = 0.8);

    // ========== 形状检测 ==========

    // 检测边缘
    static std::vector<Point> detectEdges(const Rect& region = {}, int threshold1 = 50, int threshold2 = 150);

    // 检测轮廓
    static std::vector<std::vector<Point>> detectContours(const Rect& region = {});

    // 检测圆形
    static std::vector<std::pair<Point, int>> detectCircles(const Rect& region = {}, int minRadius = 10, int maxRadius = 100);

    // ========== 图像处理 ==========

    // 截取屏幕区域为图像
    static bool captureRegion(const Rect& region, const std::string& outputPath);

    // 保存图像（用于调试）
    static bool saveImage(const std::string& path, const Bitmap& bitmap);

    // 比较两张图片的相似度
    static double compareImages(const std::string& path1, const std::string& path2);

    // 辅助函数：颜色是否在容差范围内
    static bool isColorMatch(const Color& c1, const Color& c2, int tolerance);

private:
};

} // namespace wingman

#endif // _WIN32
