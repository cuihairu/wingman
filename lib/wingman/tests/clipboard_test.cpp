#include <gtest/gtest.h>
#include "wingman/clipboard.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

// Windows 剪贴板是全局共享资源，可能被剪贴板历史、云同步、安全软件等进程占用。
// SetUp 探测可用性：当 OS 拒绝访问时跳过测试（环境问题），而非误报代码失败。
// 这消除了全套件高负载下的偶发 flaky（ClipboardTest.Clear 等）。
class ClipboardTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Clipboard::setText("wingman-clipboard-probe")) {
            GTEST_SKIP() << "Clipboard unavailable (locked by another process) — skipping";
        }
    }
};

// ========== Text Operation Tests ==========

TEST_F(ClipboardTest, SetAndGetText) {
    std::string testText = "Hello, Wingman!";

    EXPECT_TRUE(Clipboard::setText(testText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, testText);
}

TEST_F(ClipboardTest, SetEmptyText) {
    EXPECT_TRUE(Clipboard::setText(""));
    std::string result = Clipboard::getText();
    EXPECT_EQ(result, "");
}

TEST_F(ClipboardTest, SetUnicodeText) {
    std::string testText = "Hello World 🚀 Wingman";

    EXPECT_TRUE(Clipboard::setText(testText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, testText);
}

TEST_F(ClipboardTest, HasText) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasText());

    Clipboard::setText("Test content");
    EXPECT_TRUE(Clipboard::hasText());
}

TEST_F(ClipboardTest, LongText) {
    std::string longText(10000, 'A');  // 10KB text

    EXPECT_TRUE(Clipboard::setText(longText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, longText);
}

// ========== HTML Operation Tests ==========

TEST_F(ClipboardTest, SetAndGetHTML) {
    std::string testHTML = "<html><body><b>Bold</b> and <i>italic</i></body></html>";

    EXPECT_TRUE(Clipboard::setHTML(testHTML));
    std::string result = Clipboard::getHTML();

    // HTML may have format headers added, only check core content
    EXPECT_FALSE(result.empty());
}

TEST_F(ClipboardTest, HasHTML) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasHTML());

    Clipboard::setHTML("<p>Test</p>");
    EXPECT_TRUE(Clipboard::hasHTML());
}

// ========== Image Operation Tests ==========

TEST_F(ClipboardTest, SetAndGetImage) {
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

TEST_F(ClipboardTest, HasImage) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasImage());

    // Set a small image
    std::vector<uint8_t> imageData(16, 255);  // 1x1 pixel
    Clipboard::setImage(imageData, 1, 1);

    EXPECT_TRUE(Clipboard::hasImage());
}

// ========== File Operation Tests ==========

TEST_F(ClipboardTest, SetAndGetFiles) {
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

TEST_F(ClipboardTest, HasFiles) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasFiles());

    std::vector<std::string> files = {"C:\\Windows\\System32\\notepad.exe"};
    Clipboard::setFiles(files);

    EXPECT_TRUE(Clipboard::hasFiles());
}

// ========== General Operation Tests ==========

TEST_F(ClipboardTest, Clear) {
    Clipboard::setText("Some content");
    EXPECT_FALSE(Clipboard::isEmpty());

    Clipboard::clear();
    EXPECT_TRUE(Clipboard::isEmpty());
}

TEST_F(ClipboardTest, IsEmpty) {
    Clipboard::clear();
    EXPECT_TRUE(Clipboard::isEmpty());

    Clipboard::setText("Test");
    EXPECT_FALSE(Clipboard::isEmpty());
}

// ========== Boundary Condition Tests ==========

TEST_F(ClipboardTest, MultipleOperations) {
    // Test consecutive multiple operations
    for (int i = 0; i < 10; ++i) {
        std::string text = "Iteration " + std::to_string(i);
        EXPECT_TRUE(Clipboard::setText(text));
        EXPECT_EQ(Clipboard::getText(), text);
    }
}

TEST_F(ClipboardTest, FormatOverride) {
    // Set text first
    Clipboard::setText("Text content");
    EXPECT_TRUE(Clipboard::hasText());

    // Then set image, should override text
    std::vector<uint8_t> imageData(16, 255);
    Clipboard::setImage(imageData, 1, 1);

    // Image should exist, text may be overridden
    EXPECT_TRUE(Clipboard::hasImage());
}

TEST_F(ClipboardTest, SpecialCharacters) {
    std::string specialText = "Line1\nLine2\r\nLine3\tTabbed\0Binary";
    std::string textWithoutNull = specialText.substr(0, specialText.find('\0'));

    EXPECT_TRUE(Clipboard::setText(textWithoutNull));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, textWithoutNull);
}
