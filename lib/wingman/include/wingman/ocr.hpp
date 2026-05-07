#pragma once

#include <string>
#include <vector>
#include "wingman/screen.hpp"

namespace wingman {

// OCR 识别结果
struct OcrResult {
    bool success;
    std::string text;
    double confidence;
    std::vector<Rect> charRegions;
};

// OCR 文字识别模块
class OCR {
public:
    // 初始化 OCR 引擎
    static bool init(const std::string& datapath = "", const std::string& language = "eng");

    // 清理 OCR 引擎
    static void cleanup();

    // 识别屏幕区域的文字
    static OcrResult recognize(const Rect& region = {});

    // 识别图像文件的文字
    static OcrResult recognizeImage(const std::string& imagePath);

    // 识别指定 Bitmap 的文字
    static OcrResult recognizeBitmap(const Bitmap& bitmap);

    // 设置识别语言
    static bool setLanguage(const std::string& language);

    // 设置识别模式 (page seg mode)
    enum class PageSegMode {
        OSD_ONLY = 0,
        AUTO_OSD = 1,
        AUTO_ONLY = 2,
        AUTO = 3,
        SINGLE_COLUMN = 4,
        SINGLE_BLOCK_VERT_TEXT = 5,
        SINGLE_BLOCK = 6,
        SINGLE_LINE = 7,
        SINGLE_WORD = 8,
        CIRCLE_WORD = 9,
        SINGLE_CHAR = 10,
        SPARSE_TEXT = 11,
        SPARSE_TEXT_OSD = 12
    };
    static void setPageSegMode(PageSegMode mode);

    // 获取版本信息
    static std::string getVersion();

private:
    static bool initialized;
    static std::string datapath;
    static std::string language;
    static PageSegMode pageSegMode;
};

} // namespace wingman
