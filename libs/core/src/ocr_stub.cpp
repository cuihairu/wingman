#include "wingman/ocr.hpp"
#include <spdlog/spdlog.h>

// Tesseract OCR stub implementation
// When WINGMAN_ENABLE_OCR is not defined, provide stub functions

namespace wingman {

// 静态成员初始化
bool OCR::initialized = false;
std::string OCR::datapath;
std::string OCR::language;
OCR::PageSegMode OCR::pageSegMode = OCR::PageSegMode::AUTO;

bool OCR::init(const std::string& datapath, const std::string& language) {
    spdlog::warn("OCR support not enabled (compile with WINGMAN_ENABLE_OCR)");
    return false;
}

void OCR::cleanup() {
    initialized = false;
}

OcrResult OCR::recognize(const Rect& region) {
    OcrResult result = {false, "", 0.0, {}};
    spdlog::warn("OCR support not enabled");
    return result;
}

OcrResult OCR::recognizeImage(const std::string& imagePath) {
    OcrResult result = {false, "", 0.0, {}};
    spdlog::warn("OCR support not enabled");
    return result;
}

OcrResult OCR::recognizeBitmap(const Bitmap& bitmap) {
    OcrResult result = {false, "", 0.0, {}};
    spdlog::warn("OCR support not enabled");
    return result;
}

bool OCR::setLanguage(const std::string& lang) {
    return false;
}

void OCR::setPageSegMode(PageSegMode mode) {
    pageSegMode = mode;
}

std::string OCR::getVersion() {
    return "OCR not enabled";
}

} // namespace wingman
