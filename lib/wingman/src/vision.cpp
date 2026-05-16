#include "wingman/vision.hpp"

#ifdef _WIN32
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <spdlog/spdlog.h>

namespace wingman {

// ========== 辅助函数 ==========

bool Vision::isColorMatch(const Color& c1, const Color& c2, int tolerance) {
    int dr = abs((int)c1.r - (int)c2.r);
    int dg = abs((int)c1.g - (int)c2.g);
    int db = abs((int)c1.b - (int)c2.b);
    return dr <= tolerance && dg <= tolerance && db <= tolerance;
}

// Bitmap 转 cv::Mat
static cv::Mat bitmapToMat(const Bitmap& bitmap) {
    if (!bitmap.getData() || bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) {
        return cv::Mat();
    }

    // Bitmap 是 BGRA 格式
    cv::Mat mat(bitmap.getHeight(), bitmap.getWidth(), CV_8UC4, (void*)bitmap.getData(), bitmap.getWidth() * 4);

    // 转换为 BGR
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);

    return bgr;
}

// ========== 颜色检测 ==========

std::optional<Point> Vision::findColor(const Color& color, const Rect& region) {
    return findColor(color, 0, region);
}

std::optional<Point> Vision::findColor(const Color& color, int tolerance, const Rect& region) {
    auto bitmap = Screen::capture();
    if (!bitmap) return std::nullopt;

    Rect searchRegion = region.isEmpty() ? Screen::getScreenBounds() : region;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return std::nullopt;

    // 裁剪搜索区域
    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat searchMat = mat(roi);

    // 遍历像素查找匹配颜色
    for (int y = 0; y < searchMat.rows; y++) {
        for (int x = 0; x < searchMat.cols; x++) {
            cv::Vec3b pixel = searchMat.at<cv::Vec3b>(y, x);
            Color pixelColor(pixel[2], pixel[1], pixel[0]);  // BGR -> RGB

            if (isColorMatch(pixelColor, color, tolerance)) {
                return Point(searchRegion.x + x, searchRegion.y + y);
            }
        }
    }

    return std::nullopt;
}

std::vector<Point> Vision::findAllColors(const Color& color, int tolerance, const Rect& region) {
    std::vector<Point> results;

    auto bitmap = Screen::capture();
    if (!bitmap) return results;

    Rect searchRegion = region.isEmpty() ? Screen::getScreenBounds() : region;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return results;

    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat searchMat = mat(roi);

    for (int y = 0; y < searchMat.rows; y++) {
        for (int x = 0; x < searchMat.cols; x++) {
            cv::Vec3b pixel = searchMat.at<cv::Vec3b>(y, x);
            Color pixelColor(pixel[2], pixel[1], pixel[0]);

            if (isColorMatch(pixelColor, color, tolerance)) {
                results.push_back(Point(searchRegion.x + x, searchRegion.y + y));
            }
        }
    }

    return results;
}

bool Vision::hasColor(const Color& color, int tolerance, const Rect& region) {
    return findColor(color, tolerance, region).has_value();
}

Color Vision::getDominantColor(const Rect& region) {
    auto bitmap = Screen::capture();
    if (!bitmap) return Color();

    Rect searchRegion = region.isEmpty() ? Screen::getScreenBounds() : region;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return Color();

    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat searchMat = mat(roi);

    // 使用 KMeans 聚类找到主色
    cv::Mat pixels = searchMat.reshape(1, searchMat.rows * searchMat.cols);
    pixels.convertTo(pixels, CV_32F);

    cv::Mat labels, centers;
    cv::kmeans(pixels, 1, labels, cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 10, 1.0),
              3, cv::KMEANS_PP_CENTERS, centers);

    cv::Vec3f dominant = centers.at<cv::Vec3f>(0, 0);
    return Color((uint8_t)dominant[2], (uint8_t)dominant[1], (uint8_t)dominant[0]);
}

// ========== 图像匹配 ==========

ImageMatch Vision::findImage(const std::string& templatePath, double threshold) {
    return findImage(templatePath, Screen::getScreenBounds(), threshold);
}

ImageMatch Vision::findImage(const std::string& templatePath, const Rect& searchRegion, double threshold) {
    ImageMatch result = {false, Point(), 0.0, Rect()};

    // 加载模板图像
    cv::Mat templateImg = cv::imread(templatePath, cv::IMREAD_COLOR);
    if (templateImg.empty()) {
        spdlog::error("Failed to load template image: {}", templatePath);
        return result;
    }

    // 截取屏幕
    auto bitmap = Screen::capture();
    if (!bitmap) return result;

    cv::Mat screenMat = bitmapToMat(*bitmap);
    if (screenMat.empty()) return result;

    // 裁剪搜索区域
    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > screenMat.cols) roi.width = screenMat.cols - roi.x;
    if (roi.y + roi.height > screenMat.rows) roi.height = screenMat.rows - roi.y;
    cv::Mat searchMat = screenMat(roi);

    // 模板匹配
    cv::Mat matchResult;
    cv::matchTemplate(searchMat, templateImg, matchResult, cv::TM_CCOEFF_NORMED);

    // 查找最佳匹配
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

    if (maxVal >= threshold) {
        result.found = true;
        result.position = Point(searchRegion.x + maxLoc.x, searchRegion.y + maxLoc.y);
        result.confidence = maxVal;
        result.region = Rect(result.position.x, result.position.y, templateImg.cols, templateImg.rows);
    }

    return result;
}

std::vector<ImageMatch> Vision::findAllImages(const std::string& templatePath, double threshold) {
    std::vector<ImageMatch> results;

    cv::Mat templateImg = cv::imread(templatePath, cv::IMREAD_COLOR);
    if (templateImg.empty()) {
        spdlog::error("Failed to load template image: {}", templatePath);
        return results;
    }

    auto bitmap = Screen::capture();
    if (!bitmap) return results;

    cv::Mat screenMat = bitmapToMat(*bitmap);
    if (screenMat.empty()) return results;

    cv::Mat matchResult;
    cv::matchTemplate(screenMat, templateImg, matchResult, cv::TM_CCOEFF_NORMED);

    // 查找所有超过阈值的匹配
    while (true) {
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

        if (maxVal < threshold) break;

        ImageMatch match;
        match.found = true;
        match.position = Point(maxLoc.x, maxLoc.y);
        match.confidence = maxVal;
        match.region = Rect(maxLoc.x, maxLoc.y, templateImg.cols, templateImg.rows);
        results.push_back(match);

        // 抑制已找到的区域
        int suppressRadius = templateImg.cols / 2;
        int x1 = std::max(0, maxLoc.x - suppressRadius);
        int y1 = std::max(0, maxLoc.y - suppressRadius);
        int x2 = std::min(matchResult.cols, maxLoc.x + suppressRadius);
        int y2 = std::min(matchResult.rows, maxLoc.y + suppressRadius);
        cv::rectangle(matchResult, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0), -1);
    }

    return results;
}

bool Vision::waitForImage(const std::string& templatePath, int timeoutMs, double threshold) {
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto result = findImage(templatePath, threshold);
        if (result.found) return true;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ========== 形状检测 ==========

std::vector<Point> Vision::detectEdges(const Rect& region, int threshold1, int threshold2) {
    std::vector<Point> edges;

    auto bitmap = Screen::capture();
    if (!bitmap) return edges;

    Rect searchRegion = region.isEmpty() ? Screen::getScreenBounds() : region;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return edges;

    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat searchMat = mat(roi);

    // 转灰度
    cv::Mat gray;
    cv::cvtColor(searchMat, gray, cv::COLOR_BGR2GRAY);

    // Canny 边缘检测
    cv::Mat edgeMat;
    cv::Canny(gray, edgeMat, threshold1, threshold2);

    // 提取边缘点
    for (int y = 0; y < edgeMat.rows; y++) {
        for (int x = 0; x < edgeMat.cols; x++) {
            if (edgeMat.at<uint8_t>(y, x) > 0) {
                edges.push_back(Point(searchRegion.x + x, searchRegion.y + y));
            }
        }
    }

    return edges;
}

std::vector<std::vector<Point>> Vision::detectContours(const Rect& region) {
    std::vector<std::vector<Point>> contours;

    auto bitmap = Screen::capture();
    if (!bitmap) return contours;

    Rect searchRegion = region.isEmpty() ? Screen::getScreenBounds() : region;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return contours;

    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat searchMat = mat(roi);

    // 转灰度
    cv::Mat gray;
    cv::cvtColor(searchMat, gray, cv::COLOR_BGR2GRAY);

    // 二值化
    cv::Mat binary;
    cv::threshold(gray, binary, 128, 255, cv::THRESH_BINARY);

    // 查找轮廓
    std::vector<std::vector<cv::Point>> cvContours;
    cv::Mat hierarchy;
    cv::findContours(binary, cvContours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 转换轮廓格式
    for (const auto& cvContour : cvContours) {
        std::vector<Point> contour;
        for (const auto& pt : cvContour) {
            contour.push_back(Point(searchRegion.x + pt.x, searchRegion.y + pt.y));
        }
        contours.push_back(contour);
    }

    return contours;
}

std::vector<std::pair<Point, int>> Vision::detectCircles(const Rect& region, int minRadius, int maxRadius) {
    std::vector<std::pair<Point, int>> circles;

    auto bitmap = Screen::capture();
    if (!bitmap) return circles;

    Rect searchRegion = region.isEmpty() ? Screen::getScreenBounds() : region;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return circles;

    cv::Rect roi(searchRegion.x, searchRegion.y, searchRegion.width, searchRegion.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat searchMat = mat(roi);

    // 转灰度
    cv::Mat gray;
    cv::cvtColor(searchMat, gray, cv::COLOR_BGR2GRAY);

    // 高斯模糊
    cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2, 2);

    // 霍夫圆检测
    std::vector<cv::Vec3f> cvCircles;
    cv::HoughCircles(gray, cvCircles, cv::HOUGH_GRADIENT, 1, gray.rows / 8, 100, 30, minRadius, maxRadius);

    // 转换结果
    for (const auto& c : cvCircles) {
        Point center(searchRegion.x + (int)c[0], searchRegion.y + (int)c[1]);
        int radius = (int)c[2];
        circles.push_back({center, radius});
    }

    return circles;
}

// ========== 图像处理 ==========

bool Vision::captureRegion(const Rect& region, const std::string& outputPath) {
    auto bitmap = Screen::capture();
    if (!bitmap) return false;

    cv::Mat mat = bitmapToMat(*bitmap);
    if (mat.empty()) return false;

    cv::Rect roi(region.x, region.y, region.width, region.height);
    if (roi.x + roi.width > mat.cols) roi.width = mat.cols - roi.x;
    if (roi.y + roi.height > mat.rows) roi.height = mat.rows - roi.y;
    cv::Mat regionMat = mat(roi);

    return cv::imwrite(outputPath, regionMat);
}

bool Vision::saveImage(const std::string& path, const Bitmap& bitmap) {
    cv::Mat mat = bitmapToMat(bitmap);
    if (mat.empty()) return false;
    return cv::imwrite(path, mat);
}

double Vision::compareImages(const std::string& path1, const std::string& path2) {
    cv::Mat img1 = cv::imread(path1, cv::IMREAD_COLOR);
    cv::Mat img2 = cv::imread(path2, cv::IMREAD_COLOR);

    if (img1.empty() || img2.empty()) {
        spdlog::error("Failed to load images for comparison");
        return 0.0;
    }

    // 调整大小到相同尺寸
    if (img1.size() != img2.size()) {
        cv::resize(img2, img2, img1.size());
    }

    // 计算结构相似性 (SSIM)
    cv::Mat img1Gray, img2Gray;
    cv::cvtColor(img1, img1Gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(img2, img2Gray, cv::COLOR_BGR2GRAY);

    cv::Mat score;
    cv::matchTemplate(img1Gray, img2Gray, score, cv::TM_CCOEFF_NORMED);

    double minVal, maxVal;
    cv::minMaxLoc(score, &minVal, &maxVal);

    return maxVal;
}

} // namespace wingman

#endif // _WIN32
