// UpdatePoller 状态机单测：全量注入假传输，不触网。
// 覆盖轮询比较口径、候选变更只上报一次、失败不清候选、下载幂等与防重入、
// url 解析与落盘名净化。

#include "wingman/update_poller.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <future>
#include <thread>

namespace {

wingman::UpdateManifest makeManifest(const std::string& version,
                                     const std::string& url = "/api/v1/update/download/1",
                                     const std::string& sha = "0123456789abcdef",
                                     const std::string& fileName = "") {
    wingman::UpdateManifest m;
    m.version = version;
    m.url = url;
    m.sha256 = sha;
    m.size = 42;
    m.notes = "notes";
    m.mandatory = false;
    m.fileName = fileName;
    return m;
}

wingman::UpdatePollConfig makeConfig(const std::string& baseUrl,
                                     const std::string& localVersion,
                                     const std::string& downloadDir) {
    wingman::UpdatePollConfig cfg;
    cfg.baseUrl = baseUrl;
    cfg.channel = "stable";
    cfg.platform = "linux";
    cfg.arch = "amd64";
    cfg.localVersion = localVersion;
    cfg.downloadDir = downloadDir;
    return cfg;
}

} // namespace

// ========== resolveArtifactUrl ==========

TEST(UpdatePollerResolveUrl, RelativeJoinedWithBase) {
    EXPECT_EQ(wingman::resolveArtifactUrl("http://srv:9527/", "/api/v1/update/download/7"),
              "http://srv:9527/api/v1/update/download/7");
    EXPECT_EQ(wingman::resolveArtifactUrl("http://srv:9527", "/x"), "http://srv:9527/x");
}

TEST(UpdatePollerResolveUrl, AbsolutePassesThrough) {
    EXPECT_EQ(wingman::resolveArtifactUrl("http://srv", "https://other/x"), "https://other/x");
    EXPECT_EQ(wingman::resolveArtifactUrl("http://srv", "http://plain/x"), "http://plain/x");
}

TEST(UpdatePollerResolveUrl, MissingLeadingSlashAndEmptyInputs) {
    EXPECT_EQ(wingman::resolveArtifactUrl("http://srv", "api/x"), "http://srv/api/x");
    EXPECT_EQ(wingman::resolveArtifactUrl("http://srv", ""), "");
    EXPECT_EQ(wingman::resolveArtifactUrl("", "/x"), "/x");
}

// ========== artifactFileName ==========

TEST(UpdatePollerFileName, UsesManifestFileName) {
    const auto m = makeManifest("1.2.3", "/api/v1/update/download/1", "0123", "wingman-1.2.3-setup.exe");
    EXPECT_EQ(wingman::artifactFileName(m, m.version), "wingman-1.2.3-setup.exe");
}

TEST(UpdatePollerFileName, StripsDirectoryComponents) {
    const auto m = makeManifest("1.2.3", "/api/v1/update/download/1", "0123", "C:\\pkg\\setup.exe");
    EXPECT_EQ(wingman::artifactFileName(m, m.version), "setup.exe");
}

TEST(UpdatePollerFileName, RejectsTraversal) {
    const auto m = makeManifest("1.2.3", "/api/v1/update/download/1", "0123", "../evil.exe");
    EXPECT_EQ(wingman::artifactFileName(m, m.version), "evil.exe");
}

TEST(UpdatePollerFileName, FallsBackWhenUrlSegmentIsNumericId) {
    // 服务端下载 url 末段是发布 ID 数字，不带扩展名，不可用。
    const auto m = makeManifest("1.2.3", "/api/v1/update/download/12", "0123", "");
    EXPECT_EQ(wingman::artifactFileName(m, m.version), "wingman-1.2.3");
}

TEST(UpdatePollerFileName, EmptyEverywhereUsesVersionFallback) {
    const auto m = makeManifest("1.2.3", "", "0123", "");
    EXPECT_EQ(wingman::artifactFileName(m, m.version), "wingman-1.2.3");
}

// ========== pollOnce 状态机 ==========

TEST(UpdatePoller, DisabledWithoutBaseUrl) {
    const auto cfg = makeConfig("", "1.0.0", "/tmp/u");
    wingman::UpdatePoller p(cfg);
    EXPECT_FALSE(p.pollOnce());
    const auto s = p.status();
    EXPECT_FALSE(s.enabled);
    EXPECT_FALSE(s.available);
    EXPECT_EQ(s.lastError, "update endpoint not configured");
}

TEST(UpdatePoller, PollNewerVersionBecomesAvailableOnce) {
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    wingman::UpdatePoller p(cfg);

    EXPECT_TRUE(p.pollOnce());
    auto s = p.status();
    EXPECT_TRUE(s.available);
    EXPECT_FALSE(s.downloaded);
    EXPECT_EQ(s.candidate.version, "1.1.0");
    EXPECT_GT(s.lastCheckMs, 0);
    EXPECT_TRUE(s.lastError.empty());

    // 同一候选再轮询不上报，避免每周期重复发事件。
    EXPECT_FALSE(p.pollOnce());
    EXPECT_TRUE(p.status().available);
}

TEST(UpdatePoller, PollEqualOrOlderVersionStaysUnavailable) {
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [](const std::string&, const std::string&, const std::string&,
                     const std::string&, wingman::UpdateManifest& out) {
        out = makeManifest("1.0.0");
        return true;
    };
    wingman::UpdatePoller p(cfg);
    EXPECT_FALSE(p.pollOnce());
    auto s = p.status();
    EXPECT_FALSE(s.available);
    EXPECT_TRUE(s.candidate.version.empty());
    EXPECT_TRUE(s.lastError.empty());
}

TEST(UpdatePoller, CheckFailureKeepsExistingCandidate) {
    wingman::UpdateManifest remote = makeManifest("2.0.0");
    bool fail = false;
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        if (fail) {
            return false;
        }
        out = remote;
        return true;
    };
    wingman::UpdatePoller p(cfg);
    EXPECT_TRUE(p.pollOnce());
    EXPECT_TRUE(p.status().available);

    fail = true;
    EXPECT_FALSE(p.pollOnce());
    const auto s = p.status();
    EXPECT_EQ(s.candidate.version, "2.0.0");
    EXPECT_TRUE(s.available);
    EXPECT_EQ(s.lastError, "check failed (network or server unavailable)");
}

TEST(UpdatePoller, NewerVersionAfterDownloadReOpensCandidate) {
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    bool fail = false;
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        if (fail) {
            return false;
        }
        out = remote;
        return true;
    };
    cfg.downloadFn = [](const std::string&, const std::string&, const std::string&) {
        return true;
    };
    wingman::UpdatePoller p(cfg);
    EXPECT_TRUE(p.pollOnce());
    EXPECT_TRUE(p.downloadCandidate());
    EXPECT_TRUE(p.status().downloaded);
    EXPECT_FALSE(p.status().available);

    // 服务端又发布了更新版本 → 候选重新打开并只上报一次。
    remote = makeManifest("1.2.0");
    EXPECT_TRUE(p.pollOnce());
    EXPECT_TRUE(p.status().available);
    EXPECT_FALSE(p.status().downloaded);
    EXPECT_EQ(p.status().candidate.version, "1.2.0");
    EXPECT_FALSE(p.pollOnce());
}

// ========== downloadCandidate ==========

TEST(UpdatePoller, DownloadResolvesUrlAndRecordsVerifiedPath) {
    wingman::UpdateManifest remote = makeManifest("1.1.0", "/api/v1/update/download/9",
                                                   "abcd", "wingman-1.1.1-setup.exe");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv:9527/", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    std::string seenUrl;
    std::string seenDest;
    std::string seenSha;
    cfg.downloadFn = [&](const std::string& url, const std::string& dest,
                         const std::string& sha) {
        seenUrl = url;
        seenDest = dest;
        seenSha = sha;
        return true;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.pollOnce());

    EXPECT_TRUE(p.downloadCandidate());
    EXPECT_EQ(seenUrl, "http://srv:9527/api/v1/update/download/9");
    EXPECT_EQ(seenDest, "/tmp/u/wingman-1.1.1-setup.exe");
    EXPECT_EQ(seenSha, "abcd");
    const auto s = p.status();
    EXPECT_TRUE(s.downloaded);
    EXPECT_FALSE(s.available);
    EXPECT_EQ(s.downloadPath, "/tmp/u/wingman-1.1.1-setup.exe");
    EXPECT_TRUE(s.lastError.empty());

    // 幂等：重复下载不重新拉取。
    EXPECT_TRUE(p.downloadCandidate());
    EXPECT_EQ(seenUrl, "http://srv:9527/api/v1/update/download/9");
}

TEST(UpdatePoller, DownloadFailureKeepsCandidateRetryable) {
    wingman::UpdateManifest remote = makeManifest("1.1.0", "/api/v1/update/download/9", "abcd");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    cfg.downloadFn = [](const std::string&, const std::string&, const std::string&) {
        return false;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.pollOnce());

    EXPECT_FALSE(p.downloadCandidate());
    const auto s = p.status();
    EXPECT_TRUE(s.available);
    EXPECT_FALSE(s.downloaded);
    EXPECT_TRUE(s.downloadPath.empty());
    EXPECT_EQ(s.lastError, "download failed (transfer or sha256 mismatch)");
}

TEST(UpdatePoller, DownloadRejectedWithoutCandidateOrDir) {
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    wingman::UpdatePoller p(cfg);
    EXPECT_FALSE(p.downloadCandidate());
    EXPECT_EQ(p.status().lastError, "no update candidate to download");

    wingman::UpdateManifest remote = makeManifest("1.1.0");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    wingman::UpdatePoller p2(cfg);
    ASSERT_TRUE(p2.pollOnce());
    cfg.downloadDir.clear();
    wingman::UpdatePoller p3(cfg);
    p3.pollOnce();
    EXPECT_FALSE(p3.downloadCandidate());
    EXPECT_EQ(p3.status().lastError, "download directory not configured");
}

TEST(UpdatePoller, DownloadRefusesConcurrentRun) {
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    std::promise<void> release;
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    cfg.downloadFn = [&release](const std::string&, const std::string&, const std::string&) {
        release.get_future().wait();
        return true;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.pollOnce());

    auto worker = std::async(std::launch::async, [&p] { return p.downloadCandidate(); });
    // 等下载线程进入 transferring 阶段（锁已释放）。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(p.downloadCandidate());
    EXPECT_EQ(p.status().lastError, "download already in progress");

    release.set_value();
    EXPECT_TRUE(worker.get());
    EXPECT_TRUE(p.status().downloaded);
}

// ========== toJson ==========

TEST(UpdatePoller, StatusJsonShape) {
    wingman::UpdateManifest remote = makeManifest("1.1.0", "/api/v1/update/download/9",
                                                   "abcd", "wingman-1.1.0.exe");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    cfg.downloadFn = [](const std::string&, const std::string&, const std::string&) {
        return true;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.pollOnce());
    ASSERT_TRUE(p.downloadCandidate());

    const auto j = p.status().toJson();
    EXPECT_TRUE(j["enabled"].get<bool>());
    EXPECT_TRUE(j["downloaded"].get<bool>());
    EXPECT_FALSE(j["available"].get<bool>());
    EXPECT_FALSE(j["downloading"].get<bool>());
    EXPECT_EQ(j["localVersion"].get<std::string>(), "1.0.0");
    EXPECT_EQ(j["downloadPath"].get<std::string>(), "/tmp/u/wingman-1.1.0.exe");
    EXPECT_GT(j["lastCheckMs"].get<int64_t>(), 0);
    EXPECT_TRUE(j["lastError"].get<std::string>().empty());
    const auto c = j["candidate"];
    EXPECT_EQ(c["version"].get<std::string>(), "1.1.0");
    EXPECT_EQ(c["url"].get<std::string>(), "/api/v1/update/download/9");
    EXPECT_EQ(c["sha256"].get<std::string>(), "abcd");
    EXPECT_EQ(c["size"].get<int64_t>(), 42);
    EXPECT_EQ(c["fileName"].get<std::string>(), "wingman-1.1.0.exe");
}

// ========== reset ==========

TEST(UpdatePoller, ResetClearsState) {
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.pollOnce());
    EXPECT_TRUE(p.status().available);

    p.reset();
    const auto s = p.status();
    EXPECT_TRUE(s.enabled);
    EXPECT_FALSE(s.available);
    EXPECT_FALSE(s.downloaded);
    EXPECT_TRUE(s.candidate.version.empty());
    EXPECT_EQ(s.lastCheckMs, 0);
    EXPECT_TRUE(s.lastError.empty());
}

// ========== worker 线程 ==========

// 谓词等待，超时返回 false（避免状态机异常时用例挂死）。
template <typename Pred>
bool waitFor(Pred pred) {
    for (int i = 0; i < 200; ++i) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

TEST(UpdatePoller, StartThenStopReturnsImmediatelyAndNeverPolls) {
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.pollIntervalSeconds = 60;
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    wingman::UpdatePoller p(cfg);

    ASSERT_TRUE(p.start());
    EXPECT_TRUE(p.isRunning());
    EXPECT_FALSE(p.start());  // 重复 start 无效

    // stop() 必须在唤醒立即返回，不能等满整个轮询周期。
    const auto t0 = std::chrono::steady_clock::now();
    p.stop();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(elapsed, 1000);
    EXPECT_FALSE(p.isRunning());
    EXPECT_EQ(p.status().lastCheckMs, 0);  // 一轮都没跑

    // stop 后可再次 start。
    ASSERT_TRUE(p.start());
    p.stop();
    EXPECT_FALSE(p.isRunning());
}

TEST(UpdatePoller, RequestCheckWakesWorkerWithoutWaitingForInterval) {
    int calls = 0;
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.pollIntervalSeconds = 60;  // 周期很长，验证靠唤醒而非等周期
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        ++calls;
        out = remote;
        return true;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.start());

    const auto t0 = std::chrono::steady_clock::now();
    p.requestCheck();
    const bool ok = waitFor([&p] { return p.status().lastCheckMs > 0; });
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    p.stop();
    ASSERT_TRUE(ok);
    EXPECT_LT(elapsed, 1000);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(p.status().available);
}

TEST(UpdatePoller, RequestDownloadRunsOnWorkerAndReports) {
    wingman::UpdateManifest remote = makeManifest("1.1.0", "/api/v1/update/download/9",
                                                   "abcd", "wingman-1.1.0.exe");
    std::string seenDest;
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.pollIntervalSeconds = 60;
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    cfg.downloadFn = [&](const std::string&, const std::string& dest,
                         const std::string&) {
        seenDest = dest;
        return true;
    };
    wingman::UpdatePoller p(cfg);
    ASSERT_TRUE(p.start());
    p.requestCheck();
    ASSERT_TRUE(waitFor([&p] { return p.status().available; }));

    p.requestDownload();
    const bool ok = waitFor([&p] { return p.status().downloaded; });
    p.stop();
    ASSERT_TRUE(ok);
    EXPECT_EQ(seenDest, "/tmp/u/wingman-1.1.0.exe");
    EXPECT_FALSE(p.status().available);
    EXPECT_FALSE(p.status().downloading);
}

TEST(UpdatePoller, StateCallbackFiresOncePerDistinctState) {
    int reports = 0;
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.pollIntervalSeconds = 60;
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        out = remote;
        return true;
    };
    cfg.downloadFn = [](const std::string&, const std::string&, const std::string&) {
        return true;
    };
    wingman::UpdatePoller p(cfg);
    p.setOnStateChanged([&reports](const wingman::UpdateStatus&) { ++reports; });
    ASSERT_TRUE(p.start());

    p.requestCheck();
    ASSERT_TRUE(waitFor([&p] { return p.status().available; }));
    EXPECT_EQ(reports, 1);

    // 同一候选重复检查不上报。
    p.requestCheck();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(reports, 1);

    // 下载完成 → 状态再次变化 → 第二次上报。
    p.requestDownload();
    ASSERT_TRUE(waitFor([&p] { return p.status().downloaded; }));
    EXPECT_EQ(reports, 2);

    // 服务端发布更新版本 → 第三次上报。
    remote = makeManifest("1.2.0");
    p.requestCheck();
    ASSERT_TRUE(waitFor([&p] { return p.status().candidate.version == "1.2.0"; }));
    EXPECT_EQ(reports, 3);
    p.stop();
}

TEST(UpdatePoller, RequestsIgnoredWhenStopped) {
    int calls = 0;
    wingman::UpdateManifest remote = makeManifest("1.1.0");
    wingman::UpdatePollConfig cfg = makeConfig("http://srv", "1.0.0", "/tmp/u");
    cfg.checkFn = [&](const std::string&, const std::string&, const std::string&,
                      const std::string&, wingman::UpdateManifest& out) {
        ++calls;
        out = remote;
        return true;
    };
    wingman::UpdatePoller p(cfg);
    p.requestCheck();
    p.requestDownload();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(p.status().lastCheckMs, 0);
}
