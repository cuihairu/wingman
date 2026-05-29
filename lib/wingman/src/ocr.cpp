#include "wingman/ocr.hpp"
#include <spdlog/spdlog.h>

#ifdef WINGMAN_ENABLE_OCR

// Tesseract API
#ifdef _WIN32
#include <tesseract/baseapi.h>
#pragma comment(lib, "tesseract51.lib")
#else
#include <tesseract/baseapi.h>
#endif

// Leptonica API (for pixRead/pixDestroy)
#ifdef WINGMAN_ENABLE_LEPTONICA
#include <leptonica/allheaders.h>
#endif

namespace wingman {

// Static member initialization
bool OCR::initialized = false;
std::string OCR::datapath;
std::string OCR::language;
OCR::PageSegMode OCR::pageSegMode = OCR::PageSegMode::AUTO;

bool OCR::init(const std::string& datapath, const std::string& language) {
    if (initialized) {
        spdlog::warn("OCR already initialized");
        return true;
    }

    try {
        tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();

        // If datapath not specified, try environment variable or default path
        std::string tessDataPath = datapath;
        if (tessDataPath.empty()) {
            const char* envPath = std::getenv("TESSDATA_PREFIX");
            if (envPath) {
                tessDataPath = envPath;
            } else {
                // Windows default path
                tessDataPath = "./tessdata";
            }
        }

        std::string lang = language.empty() ? "eng+chi_sim" : language;

        if (api->Init(tessDataPath.c_str(), lang.c_str()) != 0) {
            spdlog::error("Failed to initialize tesseract with datapath: {}, language: {}", tessDataPath, lang);
            delete api;
            return false;
        }

        api->End();
        delete api;

        OCR::datapath = tessDataPath;
        OCR::language = lang;
        initialized = true;

        spdlog::info("OCR initialized: datapath={}, language={}", tessDataPath, lang);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("OCR initialization failed: {}", e.what());
        return false;
    }
}

void OCR::cleanup() {
    initialized = false;
}

OcrResult OCR::recognize(const Rect& region) {
    OcrResult result = {false, "", 0.0, {}};

    if (!initialized) {
        spdlog::error("OCR not initialized");
        return result;
    }

    auto bitmap = Screen::capture();
    if (!bitmap) {
        spdlog::error("Failed to capture screen");
        return result;
    }

    return recognizeBitmap(*bitmap);
}

OcrResult OCR::recognizeImage(const std::string& imagePath) {
    OcrResult result = {false, "", 0.0, {}};

    if (!initialized) {
        spdlog::error("OCR not initialized");
        return result;
    }

    try {
        tesseract::TessBaseAPI api;
        if (api.Init(datapath.c_str(), language.c_str()) != 0) {
            spdlog::error("Failed to initialize tesseract");
            return result;
        }

        // Set page segmentation mode
        api.SetPageSegMode(static_cast<tesseract::PageSegMode>(pageSegMode));

#ifdef WINGMAN_ENABLE_LEPTONICA
        // Read image (requires Leptonica)
        Pix* image = pixRead(imagePath.c_str());
        if (!image) {
            spdlog::error("Failed to read image: {}", imagePath);
            return result;
        }

        api.SetImage(image);

        // Get recognition result
        char* outText = api.GetUTF8Text();
        if (outText) {
            result.text = outText;
            delete[] outText;
        }

        result.confidence = api.MeanTextConf();
        result.success = !result.text.empty();

        pixDestroy(&image);
#else
        // Without Leptonica, use SetImage from file
        if (!api.SetImage(imagePath.c_str())) {
            spdlog::error("Failed to set image from file: {}", imagePath);
            return result;
        }

        // Get recognition result
        char* outText = api.GetUTF8Text();
        if (outText) {
            result.text = outText;
            delete[] outText;
        }

        result.confidence = api.MeanTextConf();
        result.success = !result.text.empty();
#endif

        spdlog::debug("OCR result: text='{}', confidence={}", result.text, result.confidence);
    } catch (const std::exception& e) {
        spdlog::error("OCR recognition failed: {}", e.what());
    }

    return result;
}

OcrResult OCR::recognizeBitmap(const Bitmap& bitmap) {
    OcrResult result = {false, "", 0.0, {}};

    if (!initialized) {
        spdlog::error("OCR not initialized");
        return result;
    }

    if (!bitmap.getData() || bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) {
        spdlog::error("Invalid bitmap");
        return result;
    }

    try {
        tesseract::TessBaseAPI api;
        if (api.Init(datapath.c_str(), language.c_str()) != 0) {
            spdlog::error("Failed to initialize tesseract");
            return result;
        }

        api.SetPageSegMode(static_cast<tesseract::PageSegMode>(pageSegMode));

        // Set image data (BGR format)
        api.SetImage(bitmap.getData(), bitmap.getWidth(), bitmap.getHeight(), 4, bitmap.getWidth() * 4);

        // Get recognition result
        char* outText = api.GetUTF8Text();
        if (outText) {
            result.text = outText;
            // Remove trailing newline
            while (!result.text.empty() && (result.text.back() == '\n' || result.text.back() == '\r')) {
                result.text.pop_back();
            }
            delete[] outText;
        }

        result.confidence = api.MeanTextConf();
        result.success = !result.text.empty();

        spdlog::debug("OCR result: text='{}', confidence={}", result.text, result.confidence);
    } catch (const std::exception& e) {
        spdlog::error("OCR recognition failed: {}", e.what());
    }

    return result;
}

bool OCR::setLanguage(const std::string& lang) {
    if (!initialized) {
        spdlog::error("OCR not initialized");
        return false;
    }

    language = lang;
    return true;
}

void OCR::setPageSegMode(PageSegMode mode) {
    pageSegMode = mode;
}

std::string OCR::getVersion() {
    return std::string(tesseract::TessBaseAPI::Version());
}

} // namespace wingman

#endif

