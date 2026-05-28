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
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::OSD_ONLY), 0);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::AUTO_OSD), 1);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::AUTO_ONLY), 2);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::AUTO), 3);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SINGLE_COLUMN), 4);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SINGLE_BLOCK_VERT_TEXT), 5);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SINGLE_BLOCK), 6);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SINGLE_LINE), 7);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SINGLE_WORD), 8);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::CIRCLE_WORD), 9);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SINGLE_CHAR), 10);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SPARSE_TEXT), 11);
    EXPECT_EQ(static_cast<int>(OCR::PageSegMode::SPARSE_TEXT_OSD), 12);
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

// ========== Additional OCR Tests ==========

TEST(OcrResultTest, FieldAssignment) {
    OcrResult result{};
    result.success = true;
    result.text = "Hello World";
    result.confidence = 0.95;
    result.charRegions = {Rect(0, 0, 50, 20), Rect(55, 0, 50, 20)};

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.text, "Hello World");
    EXPECT_DOUBLE_EQ(result.confidence, 0.95);
    EXPECT_EQ(result.charRegions.size(), 2u);
}

TEST(OcrResultTest, EmptyCharRegions) {
    OcrResult result{};
    EXPECT_TRUE(result.charRegions.empty());
}

TEST(OCRTest, InitWithDataPathReturnsFalseStub) {
    EXPECT_FALSE(OCR::init("/nonexistent/tessdata"));
}

TEST(OCRTest, InitWithLanguageReturnsFalseStub) {
    EXPECT_FALSE(OCR::init("", "chi_sim"));
}

TEST(OCRTest, RecognizeWithRegionReturnsFailureStub) {
    Rect region(0, 0, 100, 100);
    auto result = OCR::recognize(region);
    EXPECT_FALSE(result.success);
}

TEST(OCRTest, RecognizeBitmapReturnsFailureStub) {
    Bitmap bmp(10, 10);
    auto result = OCR::recognizeBitmap(bmp);
    EXPECT_FALSE(result.success);
}

TEST(OCRTest, SetLanguageChineseReturnsFalseStub) {
    EXPECT_FALSE(OCR::setLanguage("chi_sim"));
}

TEST(OCRTest, CleanupMultipleTimesDoesNotCrash) {
    EXPECT_NO_THROW(OCR::cleanup());
    EXPECT_NO_THROW(OCR::cleanup());
}

TEST(OCRTest, RecognizeImageEmptyPath) {
    auto result = OCR::recognizeImage("");
    EXPECT_FALSE(result.success);
}

TEST(OCRPageSegModeTest, AllEnumValues) {
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::OSD_ONLY));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::AUTO_OSD));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::AUTO_ONLY));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::AUTO));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::SINGLE_COLUMN));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::SINGLE_BLOCK_VERT_TEXT));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::SINGLE_BLOCK));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::CIRCLE_WORD));
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::SPARSE_TEXT_OSD));
}

TEST(OCRTest, GetVersionConsistent) {
    std::string v1 = OCR::getVersion();
    std::string v2 = OCR::getVersion();
    EXPECT_EQ(v1, v2);
}
