#pragma once

#include <string>
#include <memory>
#include <vector>

namespace wingman::lab {

// ========== 实验结果 ==========

struct TestResult {
    bool success = false;
    std::string message;
    double confidence = 0.0;
    int64_t elapsedMs = 0;

    // 位置信息
    int x = 0, y = 0;
    int width = 0, height = 0;

    // 匹配点列表
    std::vector<std::pair<int, int>> matches;
};

// ========== 参数配置 ==========

struct BaseConfig {
    virtual ~BaseConfig() = default;
};

struct CaptureConfig : BaseConfig {
    int x = 0, y = 0;
    int width = 0, height = 0;  // 0 = 全屏
    bool followCursor = false;
};

struct PixelConfig : BaseConfig {
    uint32_t color = 0x000000;
    int tolerance = 10;
    int searchX = 0, searchY = 0;
    int searchWidth = 0, searchHeight = 0;
};

struct ImageConfig : BaseConfig {
    std::string templatePath;
    double threshold = 0.75;
    int searchX = 0, searchY = 0;
    int searchWidth = 0, searchHeight = 0;
    int maxMatches = 10;
};

struct OCRConfig : BaseConfig {
    std::string language = "eng+chi_sim";
    std::string datapath = "";
    std::string whitelist = "";
    std::string blacklist = "";
    bool preprocess = true;
};

struct MLConfig : BaseConfig {
    std::string modelPath = "";
    std::string inputName = "input";
    std::string outputName = "output";
    double threshold = 0.5;
};

// ========== 代码生成器 ==========

class CodeGenerator {
public:
    static std::string generateCapture(const CaptureConfig& config);
    static std::string generatePixel(const PixelConfig& config);
    static std::string generateImage(const ImageConfig& config);
    static std::string generateOCR(const OCRConfig& config);
    static std::string generateTrigger(const std::string& name, const std::string& condition, const std::string& action);
};

} // namespace wingman::lab
