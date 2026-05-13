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
 * @brief 颜色匹配结果
 */
struct ColorMatch {
    Point position;        // 匹配位置
    Color foundColor;      // 实际找到的颜色
    int distance;          // 与目标颜色的距离
    double confidence;     // 匹配置信度 (0.0 - 1.0)
};

/**
 * @brief 图像匹配结果
 */
struct ImageMatch {
    Point position;        // 匹配位置
    double confidence;     // 匹配置信度 (0.0 - 1.0)
    Rect matchedRegion;    // 匹配的区域
};

/**
 * @brief 图像分析器
 *
 * 职责：从位图中查找颜色、图像等
 */
class ImageAnalyzer {
public:
    ImageAnalyzer() = default;
    ~ImageAnalyzer() = default;

    // 禁止拷贝
    ImageAnalyzer(const ImageAnalyzer&) = delete;
    ImageAnalyzer& operator=(const ImageAnalyzer&) = delete;

    /**
     * @brief 查找单个颜色点
     * @param bitmap 要分析的位图
     * @param target 目标颜色
     * @param region 搜索区域
     * @param tolerance 容差 (0-255)
     * @return 找到的点，未找到返回 std::nullopt
     */
    std::optional<ColorMatch> findColor(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance = 0
    );

    /**
     * @brief 查找所有颜色点
     * @param bitmap 要分析的位图
     * @param target 目标颜色
     * @param region 搜索区域
     * @param tolerance 容差
     * @param maxCount 最大返回数量 (0 表示不限制)
     * @return 找到的点列表
     */
    std::vector<ColorMatch> findColors(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance = 0,
        int maxCount = 0
    );

    /**
     * @brief 查找图像
     * @param bitmap 要搜索的位图
     * @param templatePath 模板图像路径
     * @param region 搜索区域
     * @param threshold 匹配阈值 (0.0 - 1.0)
     * @return 匹配结果，未找到返回 std::nullopt
     */
    std::optional<ImageMatch> findImage(
        const Bitmap& bitmap,
        const std::string& templatePath,
        const Rect& region,
        double threshold = 0.8
    );

    /**
     * @brief 查找图像（从位图模板）
     */
    std::optional<ImageMatch> findImage(
        const Bitmap& bitmap,
        const Bitmap& templateBitmap,
        const Rect& region,
        double threshold = 0.8
    );

    /**
     * @brief 从捕获源查找颜色（便捷方法）
     * @param source 捕获源
     * @param target 目标颜色
     * @param region 搜索区域
     * @param tolerance 容差
     * @return 找到的点
     */
    std::optional<ColorMatch> findColorFromSource(
        std::shared_ptr<capture::ICaptureSource> source,
        const Color& target,
        const Rect& region,
        int tolerance = 0
    );

    /**
     * @brief 异步查找颜色
     * @param callback 完成回调
     */
    void findColorAsync(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance,
        std::function<void(std::optional<ColorMatch>)> callback
    );

    /**
     * @brief 计算颜色差异
     * @param c1 颜色1
     * @param c2 颜色2
     * @return 欧氏距离平方
     */
    static int colorDistance(const Color& c1, const Color& c2);

    /**
     * @brief 检查颜色是否匹配
     * @param c1 颜色1
     * @param c2 颜色2
     * @param tolerance 容差
     * @return 匹配返回 true
     */
    static bool colorMatches(const Color& c1, const Color& c2, int tolerance);

private:
    /**
     * @brief 获取位图的搜索区域
     */
    Rect getSearchRegion(const Bitmap& bitmap, const Rect& region);

    /**
     * @brief 检查点是否在区域内
     */
    static bool isPointInRegion(const Point& p, const Rect& r) {
        return p.x >= r.x && p.x < r.x + r.width &&
               p.y >= r.y && p.y < r.y + r.height;
    }

    /**
     * @brief 限制区域到位图边界
     */
    static Rect clampRegion(const Rect& region, const Bitmap& bitmap);
};

/**
 * @brief 模式匹配器
 *
 * 提供更高级的模式匹配功能
 */
class PatternMatcher {
public:
    /**
     * @brief 多色模式匹配
     * @param bitmap 要搜索的位图
     * @param colors 颜色列表（从左到右、从上到下）
     * @param offsets 相对位置偏移
     * @param region 搜索区域
     * @param tolerance 容差
     * @return 匹配结果
     */
    static std::optional<Point> matchColorPattern(
        const Bitmap& bitmap,
        const std::vector<Color>& colors,
        const std::vector<Point>& offsets,
        const Rect& region,
        int tolerance = 0
    );

    /**
     * @brief 模板匹配（归一化交叉相关）
     */
    static double templateMatch(
        const Bitmap& bitmap,
        const Bitmap& tpl,
        Point& bestPosition,
        const Rect& region = {}
    );

    /**
     * @brief 边缘检测匹配
     */
    static std::optional<Point> matchEdge(
        const Bitmap& bitmap,
        const Rect& region,
        bool horizontal = true,
        int threshold = 128
    );
};

} // namespace wingman::vision
