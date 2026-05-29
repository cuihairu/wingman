#pragma once

#include <string>
#include <vector>
#include "wingman/screen.hpp"

namespace wingman {

// OCR recognition result
struct OcrResult {
    bool success;
    std::string text;
    double confidence;
    std::vector<Rect> charRegions;
};

// OCR text recognition module
class OCR {
public:
    // Initialize OCR engine
    static bool init(const std::string& datapath = "", const std::string& language = "eng");

    // Cleanup OCR engine
    static void cleanup();

    // Recognize text in screen region
    static OcrResult recognize(const Rect& region = {});

    // Recognize text in image file
    static OcrResult recognizeImage(const std::string& imagePath);

    // Recognize text in specified Bitmap
    static OcrResult recognizeBitmap(const Bitmap& bitmap);

    // Set recognition language
    static bool setLanguage(const std::string& language);

    // Set recognition mode (page seg mode)
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

    // Get version info
    static std::string getVersion();

private:
    static bool initialized;
    static std::string datapath;
    static std::string language;
    static PageSegMode pageSegMode;
};

} // namespace wingman
