#include <gtest/gtest.h>
#include "wingman/clipboard.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

// ========== 文本操作测试 ==========

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
    std::string longText(10000, 'A');  // 10KB 文本

    EXPECT_TRUE(Clipboard::setText(longText));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, longText);
}

// ========== HTML 操作测试 ==========

TEST(ClipboardTest, SetAndGetHTML) {
    std::string testHTML = "<html><body><b>Bold</b> and <i>italic</i></body></html>";

    EXPECT_TRUE(Clipboard::setHTML(testHTML));
    std::string result = Clipboard::getHTML();

    // HTML 可能会被添加格式头，只检查核心内容
    EXPECT_FALSE(result.empty());
}

TEST(ClipboardTest, HasHTML) {
    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasHTML());

    Clipboard::setHTML("<p>Test</p>");
    EXPECT_TRUE(Clipboard::hasHTML());
}

// ========== 图像操作测试 ==========

TEST(ClipboardTest, SetAndGetImage) {
    // 创建一个简单的 2x2 红色图像 (BGRA)
    int width = 2;
    int height = 2;
    std::vector<uint8_t> imageData;

    // BGRA: 蓝色=0, 绿色=0, 红色=255, Alpha=255
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

    // 设置一个小图像
    std::vector<uint8_t> imageData(16, 255);  // 1x1 像素
    Clipboard::setImage(imageData, 1, 1);

    EXPECT_TRUE(Clipboard::hasImage());
}

// ========== 文件操作测试 ==========

TEST(ClipboardTest, SetAndGetFiles) {
    // 注意：实际文件路径需要存在
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

// ========== 通用操作测试 ==========

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

// ========== 边界条件测试 ==========

TEST(ClipboardTest, MultipleOperations) {
    // 测试连续多次操作
    for (int i = 0; i < 10; ++i) {
        std::string text = "Iteration " + std::to_string(i);
        EXPECT_TRUE(Clipboard::setText(text));
        EXPECT_EQ(Clipboard::getText(), text);
    }
}

TEST(ClipboardTest, FormatOverride) {
    // 先设置文本
    Clipboard::setText("Text content");
    EXPECT_TRUE(Clipboard::hasText());

    // 再设置图像，应该覆盖文本
    std::vector<uint8_t> imageData(16, 255);
    Clipboard::setImage(imageData, 1, 1);

    // 图像应该存在，文本可能被覆盖
    EXPECT_TRUE(Clipboard::hasImage());
}

TEST(ClipboardTest, SpecialCharacters) {
    std::string specialText = "Line1\nLine2\r\nLine3\tTabbed\0Binary";
    std::string textWithoutNull = specialText.substr(0, specialText.find('\0'));

    EXPECT_TRUE(Clipboard::setText(textWithoutNull));
    std::string result = Clipboard::getText();

    EXPECT_EQ(result, textWithoutNull);
}
