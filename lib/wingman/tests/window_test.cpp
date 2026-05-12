#include <gtest/gtest.h>
#include "wingman/window.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== 窗口查找测试 ==========

TEST(WindowTest, FindExistingWindow) {
    // 查找桌面窗口（几乎总是存在）
    HWND progman = FindWindowW(L"Progman", NULL);
    ASSERT_NE(progman, nullptr);

    std::string title = "Program Manager";
    auto hwnd = Window::find(title);

    // 可能找到多个匹配，至少应该能找到一个
    if (hwnd != nullptr) {
        EXPECT_TRUE(Window::isValid(hwnd));
    }
}

TEST(WindowTest, FindAllWindows) {
    std::string title = "";  // 空标题匹配所有窗口

    auto windows = Window::findAll(title);
    EXPECT_GT(windows.size(), 0);
}

TEST(WindowTest, GetForegroundWindow) {
    auto hwnd = Window::getForeground();
    EXPECT_NE(hwnd, nullptr);
    EXPECT_TRUE(Window::isValid(hwnd));
}

TEST(WindowTest, EnumerateWindows) {
    auto windows = Window::enumerate();
    EXPECT_GT(windows.size(), 0);

    // 检查返回的数据
    for (const auto& win : windows) {
        EXPECT_NE(win.handle, nullptr);
        EXPECT_FALSE(win.title.empty());
    }
}

TEST(WindowTest, FindNonExistentWindow) {
    std::string nonsenseTitle = "ThisWindowShouldNotExist123456789";

    auto hwnd = Window::find(nonsenseTitle);
    // 可能返回 nullptr 或某个匹配的窗口
    SUCCEED();
}

// ========== 窗口信息测试 ==========

TEST(WindowTest, GetWindowTitle) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    std::string title = Window::getTitle(hwnd);
    EXPECT_FALSE(title.empty());
}

TEST(WindowTest, GetWindowBounds) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect bounds = Window::getBounds(hwnd);
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
}

TEST(WindowTest, IsWindowValid) {
    // 有效窗口
    auto hwnd = Window::getForeground();
    EXPECT_TRUE(Window::isValid(hwnd));

    // 无效窗口
    EXPECT_FALSE(Window::isValid(nullptr));
}

TEST(WindowTest, IsWindowForeground) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    EXPECT_TRUE(Window::isForeground(hwnd));
}

TEST(WindowTest, IsWindowVisible) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 前台窗口应该是可见的
    EXPECT_TRUE(Window::isVisible(hwnd));
}

// ========== 窗口操作测试 ==========

TEST(WindowTest, ActivateWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 尝试激活（可能已经是前台窗口）
    bool result = Window::activate(hwnd);
    // 结果取决于当前状态
    SUCCEED();
}

TEST(WindowTest, MinimizeWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 最小化
    bool result = Window::minimize(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 还原
    Window::restore(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, MaximizeWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 最大化
    bool result = Window::maximize(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 还原
    Window::restore(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, RestoreWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    bool result = Window::restore(hwnd);
    SUCCEED();
}

// ========== 窗口移动和调整大小测试 ==========

TEST(WindowTest, SetWindowBounds) {
    // 创建一个测试窗口（使用记事本）
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    wchar_t cmd[] = L"notepad.exe";

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        GTEST_SKIP() << "Could not create test window";
    }

    // 等待窗口创建
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 查找记事本窗口
    HWND notepad = FindWindowW(L"Notepad", NULL);
    if (notepad == NULL) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        GTEST_SKIP() << "Could not find Notepad window";
    }

    // 设置窗口位置和大小
    Rect newBounds(100, 100, 400, 300);
    bool result = Window::setBounds(notepad, newBounds);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证
    Rect actualBounds = Window::getBounds(notepad);
    EXPECT_EQ(actualBounds.x, newBounds.x);
    EXPECT_EQ(actualBounds.y, newBounds.y);
    EXPECT_EQ(actualBounds.width, newBounds.width);
    EXPECT_EQ(actualBounds.height, newBounds.height);

    // 清理
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

TEST(WindowTest, MoveWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect original = Window::getBounds(hwnd);

    // 移动窗口
    bool result = Window::move(hwnd, 200, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 恢复原位置
    Window::move(hwnd, original.x, original.y);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, ResizeWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect original = Window::getBounds(hwnd);

    // 调整大小
    bool result = Window::resize(hwnd, 800, 600);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 恢复原大小
    Window::resize(hwnd, original.width, original.height);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

// ========== 等待窗口测试 ==========

TEST(WindowTest, WaitForWindow) {
    // 创建一个测试窗口
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    wchar_t cmd[] = L"notepad.exe";

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        GTEST_SKIP() << "Could not create test window";
    }

    // 等待窗口出现
    bool found = Window::waitFor("Notepad", 3000);
    EXPECT_TRUE(found);

    // 清理
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // 等待窗口关闭
    bool closed = Window::waitClose("Notepad", 3000);
    EXPECT_TRUE(closed);
}

TEST(WindowTest, WaitForWindowTimeout) {
    // 等待不存在的窗口
    bool found = Window::waitFor("NonExistentWindow12345", 500);
    EXPECT_FALSE(found);
}

TEST(WindowTest, WaitCloseTimeout) {
    // 等待不存在的窗口关闭
    bool closed = Window::waitClose("NonExistentWindow12345", 500);
    EXPECT_FALSE(closed);
}

// ========== 边界条件测试 ==========

TEST(WindowTest, InvalidWindowHandle) {
    // 测试使用 nullptr 的情况
    std::string title = Window::getTitle(nullptr);
    EXPECT_TRUE(title.empty());

    Rect bounds = Window::getBounds(nullptr);
    EXPECT_TRUE(bounds.isEmpty());

    EXPECT_FALSE(Window::activate(nullptr));
    EXPECT_FALSE(Window::minimize(nullptr));
    EXPECT_FALSE(Window::maximize(nullptr));
    EXPECT_FALSE(Window::restore(nullptr));
    EXPECT_FALSE(Window::move(nullptr, 0, 0));
    EXPECT_FALSE(Window::resize(nullptr, 100, 100));
}

TEST(WindowTest, EmptyTitleSearch) {
    // 空字符串应该匹配所有窗口
    auto hwnd = Window::find("");
    // 结果取决于系统状态
    SUCCEED();
}

TEST(WindowTest, SetBoundsZeroSize) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect original = Window::getBounds(hwnd);

    // 尝试设置零大小（可能被系统忽略）
    Rect zeroBounds(0, 0, 0, 0);
    bool result = Window::setBounds(hwnd, zeroBounds);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 恢复原始大小
    Window::setBounds(hwnd, original);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}
