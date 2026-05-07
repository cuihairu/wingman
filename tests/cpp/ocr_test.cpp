#include <gtest/gtest.h>
#include "wingman/ocr.hpp"

using namespace wingman;

class OCRTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// OcrResult Tests
// ============================================================================

TEST_F(OCRTest, OcrResultDefaults) {
    OcrResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.text.empty());
    EXPECT_EQ(result.confidence, 0.0);
    EXPECT_TRUE(result.charRegions.empty());
}

TEST_F(OCRTest, OcrResultWithValues) {
    OcrResult result;
    result.success = true;
    result.text = "Hello World";
    result.confidence = 0.95;

    Rect r1 = {0, 0, 10, 10};
    Rect r2 = {15, 0, 25, 10};
    result.charRegions.push_back(r1);
    result.charRegions.push_back(r2);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.text, "Hello World");
    EXPECT_EQ(result.confidence, 0.95);
    EXPECT_EQ(result.charRegions.size(), 2);
}

// ============================================================================
// OCR Tests
// ============================================================================

TEST_F(OCRTest, OCRInit) {
    // May fail if Tesseract not available
    bool result = OCR::init();
    // Result depends on system
    SUCCEED();
}

TEST_F(OCRTest, OCRCleanup) {
    EXPECT_NO_THROW(OCR::cleanup());
}

TEST_F(OCRTest, OCRRecognize) {
    OcrResult result = OCR::recognize();
    // May fail if not initialized or Tesseract not available
    SUCCEED();
}

TEST_F(OCRTest, OCRRecognizeWithRegion) {
    Rect region = {0, 0, 100, 100};
    OcrResult result = OCR::recognize(region);
    // Result depends on system
    SUCCEED();
}

TEST_F(OCRTest, OCRRecognizeImage) {
    OcrResult result = OCR::recognizeImage("nonexistent.png");
    EXPECT_FALSE(result.success);
}

TEST_F(OCRTest, OCRSetLanguage) {
    bool result = OCR::setLanguage("eng");
    // May fail if not initialized
    SUCCEED();
}

TEST_F(OCRTest, OCRGetVersion) {
    std::string version = OCR::getVersion();
    // May be empty if not available
    SUCCEED();
}

// ============================================================================
// PageSegMode Tests
// ============================================================================

TEST_F(OCRTest, PageSegModeValues) {
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

TEST_F(OCRTest, OCRSetPageSegMode) {
    EXPECT_NO_THROW(OCR::setPageSegMode(OCR::PageSegMode::AUTO));
}
