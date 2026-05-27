#include <gtest/gtest.h>
#include "wingman/ocr.hpp"

using namespace wingman;

// ========== OcrResult ==========

TEST(OcrResultTest, DefaultValues) {
    OcrResult result{};
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.text.empty());
    EXPECT_DOUBLE_EQ(result.confidence, 0.0);
    EXPECT_TRUE(result.charRegions.empty());
}

// ========== OCR PageSegMode ==========

TEST(OCRPageSegModeTest, EnumValues) {
    EXPECT_NO_THROW(OCR::PageSegMode m = OCR::PageSegMode::OSD_ONLY);
    EXPECT_NO_THROW(OCR::PageSegMode m = OCR::PageSegMode::AUTO);
    EXPECT_NO_THROW(OCR::PageSegMode m = OCR::PageSegMode::SINGLE_LINE);
    EXPECT_NO_THROW(OCR::PageSegMode m = OCR::PageSegMode::SINGLE_WORD);
    EXPECT_NO_THROW(OCR::PageSegMode m = OCR::PageSegMode::SINGLE_CHAR);
    EXPECT_NO_THROW(OCR::PageSegMode m = OCR::PageSegMode::SPARSE_TEXT);
}

// ========== OCR (Stub) ==========

TEST(OCRTest, InitReturnsFalseStub) {
    EXPECT_FALSE(OCR::init());
}

TEST(OCRTest, CleanupDoesNotCrash) {
    EXPECT_NO_THROW(OCR::cleanup());
}

TEST(OCRTest, RecognizeReturnsFailureStub) {
    auto result = OCR::recognize();
    EXPECT_FALSE(result.success);
}

TEST(OCRTest, RecognizeImageReturnsFailureStub) {
    auto result = OCR::recognizeImage("nonexistent.png");
    EXPECT_FALSE(result.success);
}

TEST(OCRTest, SetLanguageReturnsFalseStub) {
    EXPECT_FALSE(OCR::setLanguage("eng"));
}

TEST(OCRTest, SetPageSegModeDoesNotCrash) {
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::SINGLE_LINE));
}

TEST(OCRTest, GetVersion) {
    std::string version = OCR::getVersion();
    EXPECT_FALSE(version.empty());
}
