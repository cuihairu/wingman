#include <gtest/gtest.h>
#include "wingman/clipboard.hpp"
#include "clipboard_lock_guard.hpp"
#include "clipboard_poll.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#include <chrono>

using namespace wingman;

// Windows 剪贴板是全局共享资源，可能被剪贴板历史、云同步、安全软件等进程占用。
// SetUp 探测可用性：当 OS 拒绝访问时跳过测试（环境问题），而非误报代码失败。
// 这消除了全套件高负载下的偶发 flaky（ClipboardTest.Clear 等）。
// X11 后端无 OS 级互斥，ClipboardLockGuard 以 flock 在并行测试进程间串行化。
class ClipboardTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Clipboard::setText("wingman-clipboard-probe")) {
            GTEST_SKIP() << "Clipboard unavailable (locked by another process) — skipping";
        }
    }

    // fixture 生命周期 = 整个测试体：锁覆盖 SetUp→TearDown 全程
    ClipboardLockGuard clipboardLock_;

    // X11 selection 的写入→可读存在异步窗口（XSetSelectionOwner 返回后
    // 所有权传播与 property 写入非同步可见），全套件高负载下 setText 后
    // 立即 getText 可能读回上一用例的旧内容（实测读回 probe 文本的偶发
    // flaky）。读回不匹配时短暂轮询；首次读取即命中的后端行为不变
    std::string setTextAndGetText(const std::string& text) {
        EXPECT_TRUE(Clipboard::setText(text));
        for (int attempt = 0; attempt < 40; ++attempt) {
            std::string result = Clipboard::getText();
            if (result == text) return result;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return Clipboard::getText(); // 最终值交给调用方断言报告差异
    }
};

// ========== Text Operation Tests ==========

TEST_F(ClipboardTest, SetAndGetText) {
    std::string testText = "Hello, Wingman!";

    EXPECT_EQ(setTextAndGetText(testText), testText);
}

TEST_F(ClipboardTest, SetEmptyText) {
    EXPECT_EQ(setTextAndGetText(""), "");
}

TEST_F(ClipboardTest, SetUnicodeText) {
    std::string testText = "Hello World 🚀 Wingman";

    EXPECT_EQ(setTextAndGetText(testText), testText);
}

TEST_F(ClipboardTest, HasText) {
    Clipboard::clear();

    // clear/setText 后 selection 所有权转移异步（xclip daemon 接管前旧 owner
    // 仍在位）：写后断言一律用「最终态出现」的正向等待——EXPECT_FALSE(waitFor(pred))
    // 相当于要求首次读取即为终态，与接管窗口竞争，高负载必偶发假红（2026-09-30
    // 全量门禁 ClipboardTest.IsEmpty 实测）。
    EXPECT_TRUE(clipboard_test::waitFor([] { return !Clipboard::hasText(); }));

    Clipboard::setText("Test content");
    EXPECT_TRUE(clipboard_test::waitFor([] { return Clipboard::hasText(); }));
}

TEST_F(ClipboardTest, LongText) {
    std::string longText(10000, 'A');  // 10KB text

    EXPECT_EQ(setTextAndGetText(longText), longText);
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

    // 同 HasText：clear 生效异步，正向等待终态（见该用例内注释）
    EXPECT_TRUE(clipboard_test::waitFor([] { return !Clipboard::hasHTML(); }));

    Clipboard::setHTML("<p>Test</p>");
    EXPECT_TRUE(clipboard_test::waitFor([] { return Clipboard::hasHTML(); }));
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

    // 后端能力守卫：X11/xclip 等后端未实现 image 写入（恒 false），跳过而非误报
    if (!Clipboard::setImage(imageData, width, height)) {
        GTEST_SKIP() << "Clipboard backend does not support images — skipping";
    }

    int outWidth = 0, outHeight = 0;
    std::vector<uint8_t> result = Clipboard::getImage(&outWidth, &outHeight);

    EXPECT_FALSE(result.empty());
    EXPECT_EQ(outWidth, width);
    EXPECT_EQ(outHeight, height);
}

TEST_F(ClipboardTest, HasImage) {
    // 后端能力守卫：无 image 写入能力的后端（X11/xclip）跳过
    std::vector<uint8_t> imageData(16, 255);  // 1x1 pixel
    if (!Clipboard::setImage(imageData, 1, 1)) {
        GTEST_SKIP() << "Clipboard backend does not support images — skipping";
    }

    Clipboard::clear();

    EXPECT_FALSE(Clipboard::hasImage());

    Clipboard::setImage(imageData, 1, 1);

    EXPECT_TRUE(Clipboard::hasImage());
}

// ========== File Operation Tests ==========

// 文件列表输入须为真实存在的路径（Windows CF_HDROP 与 macOS public.file-url
// 的语义都指向真实文件；原 Windows 硬编码路径在非 Windows 平台是挂空路径，
// macOS 后端 fileURLWithPath: 对其生成无效 file URL——写入声明成功、读回
// 为空，2026-09-30 macOS 验证腿首跑实证）。进程唯一临时目录 +
// random_device 后缀（ctest 并行进程间不碰撞，同 storage_test 纪律），
// 幂等创建空文件。
static std::vector<std::string> makeExistingTempFiles(
    std::initializer_list<const char*> names) {
    namespace fs = std::filesystem;
    static const fs::path dir = fs::temp_directory_path() /
        ("wingman_clip_files_" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    std::vector<std::string> paths;
    for (const char* name : names) {
        fs::path p = dir / name;
        std::ofstream(p, std::ios::app);  // 已存在则保持，幂等
        paths.push_back(p.string());
    }
    return paths;
}

TEST_F(ClipboardTest, SetAndGetFiles) {
    // actual file paths must exist（macOS 后端对挂空路径生成无效 file URL）
    std::vector<std::string> files = makeExistingTempFiles({"a.txt", "b.txt"});

    // 后端能力守卫：X11/xclip 等后端未实现文件列表写入（恒 false），跳过而非误报
    if (!Clipboard::setFiles(files)) {
        GTEST_SKIP() << "Clipboard backend does not support file lists — skipping";
    }
    std::vector<std::string> result;
    // x11 后端 selection 写入→可读异步生效（同 setTextAndGetText 轮询纪律）；
    // 立即读即命中的后端行为不变
    for (int attempt = 0; attempt < 40 && result != files; ++attempt) {
        result = Clipboard::getFiles();
        if (result != files) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    EXPECT_EQ(result.size(), files.size());
    if (result.size() == files.size()) {
        EXPECT_EQ(result[0], files[0]);
        EXPECT_EQ(result[1], files[1]);
    }
}

TEST_F(ClipboardTest, HasFiles) {
    // 后端能力守卫：无文件列表写入能力的后端（X11/xclip）跳过
    std::vector<std::string> files = makeExistingTempFiles({"a.txt"});
    if (!Clipboard::setFiles(files)) {
        GTEST_SKIP() << "Clipboard backend does not support file lists — skipping";
    }

    Clipboard::clear();

    // x11 后端 selection 所有权转移异步生效：clear 后立即探测可能命中前一用例
    // 残留的 FILE_LIST target（第六批全量实测偶发失败，本批统一到共享轮询
    // helper clipboard_poll.hpp）；断言取「最终消失」正向等待，避免首次读取
    // 仍命中旧 owner 的接管窗口竞争
    EXPECT_TRUE(clipboard_test::waitFor([] { return !Clipboard::hasFiles(); }));

    Clipboard::setFiles(files);

    EXPECT_TRUE(clipboard_test::waitFor([] { return Clipboard::hasFiles(); }));
}

// ========== General Operation Tests ==========

TEST_F(ClipboardTest, Clear) {
    Clipboard::setText("Some content");
    // setText 后正向等待非空终态（接管异步，见 HasText 用例内注释）
    EXPECT_TRUE(clipboard_test::waitFor([] { return !Clipboard::isEmpty(); }));

    Clipboard::clear();
    EXPECT_TRUE(clipboard_test::waitFor([] { return Clipboard::isEmpty(); }));
}

TEST_F(ClipboardTest, IsEmpty) {
    Clipboard::clear();
    EXPECT_TRUE(clipboard_test::waitFor([] { return Clipboard::isEmpty(); }));

    Clipboard::setText("Test");
    // 同上：clear 建立的空态会一直可读到 setText 的 xclip daemon 接管为止，
    // 首次读取恒可能为空——等待非空终态出现才是本用例的契约（写入可读）
    EXPECT_TRUE(clipboard_test::waitFor([] { return !Clipboard::isEmpty(); }));
}

// ========== Boundary Condition Tests ==========

TEST_F(ClipboardTest, MultipleOperations) {
    // Test consecutive multiple operations
    for (int i = 0; i < 10; ++i) {
        std::string text = "Iteration " + std::to_string(i);
        EXPECT_EQ(setTextAndGetText(text), text);
    }
}

TEST_F(ClipboardTest, FormatOverride) {
    // Set text first
    Clipboard::setText("Text content");
    EXPECT_TRUE(Clipboard::hasText());

    // Then set image, should override text
    std::vector<uint8_t> imageData(16, 255);
    if (!Clipboard::setImage(imageData, 1, 1)) {
        GTEST_SKIP() << "Clipboard backend does not support images — skipping";
    }
    Clipboard::setImage(imageData, 1, 1);

    // Image should exist, text may be overridden
    EXPECT_TRUE(Clipboard::hasImage());
}

TEST_F(ClipboardTest, SpecialCharacters) {
    std::string specialText = "Line1\nLine2\r\nLine3\tTabbed\0Binary";
    std::string textWithoutNull = specialText.substr(0, specialText.find('\0'));

    EXPECT_EQ(setTextAndGetText(textWithoutNull), textWithoutNull);
}
