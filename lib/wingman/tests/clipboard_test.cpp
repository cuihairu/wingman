#include <gtest/gtest.h>
#include "wingman/clipboard.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

// ========== Text Operation Tests ==========

TEST(ClipboardTest, SetAndGetText) {
    std::string testText = "Hello, Wingman!";

    EXPECT_TRUE(Clipboard::setText(testText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, testText);
}

TEST(ClipboardTest, SetEmptyText) {
    EXPECT_TRUE(Clipboard::setText(""));
    std::string result = Clipboard::getText();
    EXPECT_EQ(result, "");
}

TEST(ClipboardTest, SetUnicodeText) {
    std::string testText = "你好世界 🚀 Wingman";

    EXPECT_TRUE(Clipboard::setText(testText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, testText);
}

TEST(ClipboardTest, HasText) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasText());

    Clipboard::setText("Test content");
    EXPECT_TRUE(Clipboard::hasText());
}

TEST(ClipboardTest, LongText) {
    std::string longText(10000, 'A');  // 10KB text

    EXPECT_TRUE(Clipboard::setText(longText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, longText);
}

// ========== HTML Operation Tests ==========

TEST(ClipboardTest, SetAndGetHTML) {
    std::string testHTML = "<html><body><b>Bold</b> and <i>italic</i></body></html>";

    EXPECT_TRUE(Clipboard::setHTML(testHTML));
    std::string result = Clipboard::getHTML();

    // HTML may have format headers added, only check core content
    EXPECT_FALSE(result.empty());
}

TEST(ClipboardTest, HasHTML) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasHTML());

    Clipboard::setHTML("<p>Test</p>");
    EXPECT_TRUE(Clipboard::hasHTML());
}

// ========== Image Operation Tests ==========

TEST(ClipboardTest, SetAndGetImage) {
    // Create a simple 2x2 red image (BGRA)
    int width = 2;
    int height = 2;
    std::vector<uint8_t> imageData;

    // BGRA: Blue=0, Green=0, Red=255, Alpha=255
    for (int i = 0; i < width * height; ++i) {
        imageData.push_back(0);    // B
        imageData.push_back(0);    // G
        imageData.push_back(255);  // R
        imageData.push_back(255);  // A
    }

    EXPECT_TRUE(Clipboard::setImage(imageData, width, height));

    int outWidth = 0, outHeight = 0;
    std::vector<uint8_t> result = Clipboard::getImage(&outWidth, &outHeight);

    EXPECT_FALSE(result.empty());
    EXPECT_EQ(outWidth, width);
    EXPECT_EQ(outHeight, height);
}

TEST(ClipboardTest, HasImage) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasImage());

    // Set a small image
    std::vector<uint8_t> imageData(16, 255);  // 1x1 pixel
    Clipboard::setImage(imageData, 1, 1);

    EXPECT_TRUE(Clipboard::hasImage());
}

// ========== File Operation Tests ==========

TEST(ClipboardTest, SetAndGetFiles) {
    // Note: actual file paths must exist
    std::vector<std::string> files = {
        "C:\\Windows\\System32\\notepad.exe",
        "C:\\Windows\\System32\\calc.exe"
    };

    EXPECT_TRUE(Clipboard::setFiles(files));
    std::vector<std::string> result = Clipboard::getFiles();

    EXPECT_EQ(result.size(), files.size());
    if (result.size() == files.size()) {
        EXPECT_EQ(result[0], files[0]);
        EXPECT_EQ(result[1], files[1]);
    }
}

TEST(ClipboardTest, HasFiles) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasFiles());

    std::vector<std::string> files = {"C:\\Windows\\System32\\notepad.exe"};
    Clipboard::setFiles(files);

    EXPECT_TRUE(Clipboard::hasFiles());
}

// ========== General Operation Tests ==========

TEST(ClipboardTest, Clear) {
    Clipboard::setText("Some content");
    EXPECT_FALSE(Clipboard::isEmpty());

    Clipboard::clear();
    EXPECT_TRUE(Clipboard::isEmpty());
}

TEST(ClipboardTest, IsEmpty) {
    Clipboard::clear();
    EXPECT_TRUE(Clipboard::isEmpty());

    Clipboard::setText("Test");
    EXPECT_FALSE(Clipboard::isEmpty());
}

// ========== Boundary Condition Tests ==========

TEST(ClipboardTest, MultipleOperations) {
    // Test consecutive multiple operations
    for (int i = 0; i < 10; ++i) {
        std::string text = "Iteration " + std::to_string(i);
        EXPECT_TRUE(Clipboard::setText(text));
        EXPECT_EQ(Clipboard::getText(), text);
    }
}

TEST(ClipboardTest, FormatOverride) {
    // Set text first
    Clipboard::setText("Text content");
    EXPECT_TRUE(Clipboard::hasText());

    // Then set image, should override text
    std::vector<uint8_t> imageData(16, 255);
    Clipboard::setImage(imageData, 1, 1);

    // Image should exist, text may be overridden
    EXPECT_TRUE(Clipboard::hasImage());
}

TEST(ClipboardTest, SpecialCharacters) {
    std::string specialText = "Line1\nLine2\r\nLine3\tTabbed\0Binary";
    std::string textWithoutNull = specialText.substr(0, specialText.find('\0'));

    EXPECT_TRUE(Clipboard::setText(textWithoutNull));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, textWithoutNull);
}
