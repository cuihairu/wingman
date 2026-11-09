#if defined(__linux__)

#include "platform_x11_test_common.hpp"

// ========== 剪贴板（顶层装配断链回归守卫 + xclip 文本链路） ==========

TEST_F(X11PlatformTest, TopLevelClipboardUsesX11Backend) {
    // 装配断链回归守卫：顶层 Clipboard 在 Linux 曾恒为 NullClipboard，
    // X11Clipboard 全库零消费者；有 X 时必须装配为 X11/xclip 后端。
    EXPECT_EQ(wingman::Clipboard::instance().getBackendName(), "X11/xclip");
}

TEST_F(X11PlatformTest, ClipboardTextRoundtrip) {
    // X11 selection 无跨进程锁：与 ClipboardTest.* 并行（ctest -j）时需 flock 串行化
    ClipboardLockGuard clipboardLock;
    auto& clipboard = wingman::Clipboard::instance();
    const std::string payload = "wingman-x11-clipboard-e2e";
    if (!clipboard.setText(payload)) {
        if (!xclipAvailable()) {
            GTEST_SKIP() << "xclip not installed — setText 走优雅失败路径（运行期可选依赖）";
        }
        FAIL() << "setText failed although xclip is available";
    }
    // xclip daemon 化持 selection 是异步的：setText 返回 true 时新 owner 可能
    // 尚未接管（高负载全量跑下实测读回上一用例旧内容的偶发 flaky）
    EXPECT_TRUE(clipboard_test::waitFor([&] { return clipboard.getText() == payload; }));
    EXPECT_EQ(clipboard.getText(), payload);
    EXPECT_TRUE(clipboard.hasText());
}

TEST_F(X11PlatformTest, ClipboardFullSurfaceMethods) {
    // X11Clipboard 全接口面：图像 stub / 文件换行拼接 / 格式枚举 / clear 清空
    // （xclip 空输入 = 清空 selection，已实测验证）。fork+xclip 链路内的
    // 子进程行与 pipe/fork 失败分支不覆盖，论证见 CHANGELOG 第四批条目。
    ClipboardLockGuard clipboardLock;
    auto& clipboard = wingman::Clipboard::instance();
    if (!xclipAvailable()) {
        GTEST_SKIP() << "xclip not installed — 运行期可选依赖";
    }

    // 图像通道是 MVP 外的 stub：恒 false / 空
    EXPECT_FALSE(clipboard.setImage({1, 2, 3}, 2, 2));
    int w = 9, h = 9;
    EXPECT_TRUE(clipboard.getImage(&w, &h).empty());
    EXPECT_EQ(w, 0);
    EXPECT_EQ(h, 0);
    EXPECT_FALSE(clipboard.hasImage());

    // 文件列表：空拒绝；非空经换行拼接走文本通道，回读按行拆分
    EXPECT_FALSE(clipboard.setFiles({}));
    const std::vector<std::string> files = {"/tmp/wingman-a.txt", "/tmp/wingman-b.txt"};
    EXPECT_TRUE(clipboard.setFiles(files));
    // xclip 异步接管 selection（同 ClipboardTextRoundtrip），轮询等待回读
    EXPECT_TRUE(clipboard_test::waitFor([&] {
        const auto got = clipboard.getFiles();
        return got.size() == files.size() && got[0] == files[0] && got[1] == files[1];
    }));
    const auto got = clipboard.getFiles();
    ASSERT_EQ(got.size(), files.size());
    EXPECT_EQ(got[0], files[0]);
    EXPECT_EQ(got[1], files[1]);
    EXPECT_TRUE(clipboard.hasFiles());
    EXPECT_TRUE(clipboard.hasText());
    EXPECT_TRUE(clipboard.hasHTML());  // HTML 通道复用文本实现

    // 格式枚举 + isEmpty 与 getText 自洽
    EXPECT_FALSE(clipboard.getAvailableFormats().empty());
    EXPECT_EQ(clipboard.isEmpty(), clipboard.getText().empty());

    // clear（空 selection）后全空（clear→新 xclip 异步接管，轮询等待空态）
    clipboard.clear();
    EXPECT_TRUE(clipboard_test::waitFor([&] { return clipboard.isEmpty(); }));
    EXPECT_FALSE(clipboard.hasText());
    EXPECT_FALSE(clipboard.hasFiles());

    // 空剪贴板的格式枚举空返回 + 后端元数据（第十批补测：getBackendInfo
    // 与 hasText=false 的枚举分支此前从未触达）
    EXPECT_TRUE(clipboard.getAvailableFormats().empty());
    const auto info = clipboard.getBackendInfo();
    EXPECT_EQ(info.name, "X11/xclip");
}


#endif // __linux__
