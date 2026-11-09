#if defined(__linux__)

#include "platform_x11_test_common.hpp"

TEST_F(X11PlatformTest, InotifyBackendNameAndFileRecursiveEarlyOut) {
    auto& watcher = wingman::FileWatcher::instance();
    EXPECT_EQ(watcher.getBackendName(), "inotify");

    // 对普通文件 recursive=true：递归遍历发现根路径非目录即早退（163），
    // 文件自身的 watch 仍正常建立，随后清理
    const std::string filePath = "/tmp/wingman_batch11_inotify_probe.txt";
    {
        std::ofstream file(filePath);
        file << "probe";
    }
    const uint64_t id = watcher.watch(
        filePath, true, [](const wingman::platform::FileChange&) {});
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(watcher.unwatch(id));
    std::remove(filePath.c_str());
}

// 独立 inotify 实例：递归遍历权限跳过 + 回调异常吞噬 + shutdown 清理
TEST_F(X11PlatformTest, InotifyCallbackExceptionRecursivePermissionAndShutdownCleanup) {
    auto watcher = wingman::platform::linux::createInotifyFileWatcher();
    ASSERT_NE(watcher, nullptr);
    // 工厂内部已完成 initialize（x11_factory.cpp createInotifyFileWatcher）；
    // 幂等防护下二次调用安全返回 true，不重建 pollThread_
    EXPECT_TRUE(watcher->initialize());
    EXPECT_TRUE(watcher->getBackendInfo().isInitialized);

    namespace fs = std::filesystem;
    const std::string root = "/tmp/wingman_batch11_inotify_" + std::to_string(::getpid());
    fs::remove_all(root);
    fs::create_directories(root + "/sub");
    // 递归遍历权限错误分支（170-173）：blocked 无读权限，iterator increment
    // 报错被逐条跳过（root 用户下权限检查旁路，不触发但不影响用例语义）
    fs::create_directories(root + "/blocked");
    ::chmod((root + "/blocked").c_str(), 0000);

    // 回调先计数再抛异常 → pollThread catch 吞噬（289-293），watcher 不受影响
    std::atomic<int> callbackCalls{0};
    const uint64_t id = watcher->watch(root, /*recursive=*/true,
        [&callbackCalls](const wingman::platform::FileChange&) {
            ++callbackCalls;
            throw std::runtime_error("batch11 inotify callback boom");
        });
    ASSERT_NE(id, 0u);
    ::chmod((root + "/blocked").c_str(), 0755); // 遍历完成即恢复，避免残留

    {
        std::ofstream f(root + "/sub/probe.txt");
        f << "batch11";
    }
    // 事件派发是 pollThread 异步的，轮询等待（至多 ~3s）
    bool sawCallback = false;
    for (int i = 0; i < 30 && !sawCallback; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sawCallback = callbackCalls.load() > 0;
    }
    EXPECT_TRUE(sawCallback); // 回调确曾执行且异常被吞噬（watcher 存活可 shutdown）

    // shutdown 对活跃 watch 执行 inotify_rm_watch 清理循环（38-44）
    watcher->shutdown();
    watcher.reset();
    std::error_code ec;
    fs::remove_all(root, ec);
}

#endif // __linux__

