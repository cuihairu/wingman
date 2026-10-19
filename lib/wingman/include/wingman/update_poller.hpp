#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "wingman/update.hpp"

namespace wingman {

// 更新轮询器（ROADMAP M8.1 的「runtime 轮询触发」半）。
//
// 拥有自己的 worker 线程做周期轮询：checkUpdateLatest 走 HTTP，最坏要等
// 传输超时，绝不能阻塞本地 IPC 的 handler 线程。GUI 经 update.check /
// update.download RPC 触发即时动作，poller 在 worker 上执行并通过
// onStateChanged 回调把状态投给调用方（runtime 侧转投 EventBuffer，
// GUI 经 events.drain 拉取）。
//
// 安全口径：制品只下载到本地目录并做 sha256 校验（先校验后落名，由
// downloadUpdateArtifact 保证），本组件不执行、不替换自身进程——静默自替换
// 是各平台原生行为，需在真机桌面会话中单独验收，见 docs/development-todo.md。

// 传输注入点。签名与 update.hpp 的对应函数一致，便于默认实现直接绑定。
using UpdateCheckFn =
    std::function<bool(const std::string& baseUrl,
                       const std::string& channel,
                       const std::string& platform,
                       const std::string& arch,
                       UpdateManifest& out)>;
using UpdateDownloadFn = std::function<bool(const std::string& url,
                                            const std::string& destPath,
                                            const std::string& sha256Hex)>;

struct UpdatePollConfig {
    // Go server 的 HTTP API 基址（如 http://192.168.1.10:9527）。
    // 空 = 更新功能禁用（pollOnce 恒返回 false 并给出可读错误）。
    std::string baseUrl;
    std::string channel = "stable";
    // platform/arch 留空则用服务端默认（stable/windows/amd64）。
    std::string platform;
    std::string arch;
    // 当前运行版本（WINGMAN_VERSION），用于与 manifest 比较。
    std::string localVersion;
    // 校验通过的制品落盘目录。
    std::string downloadDir;
    // 轮询周期（秒）。< 60 收敛为 60，避免给服务端造成无意义压力。
    int pollIntervalSeconds = 3600;
    UpdateCheckFn checkFn;
    UpdateDownloadFn downloadFn;
};

// 一次轮询/下载的可见状态。toJson() 由 RPC handler 直接回给 GUI。
struct UpdateStatus {
    bool enabled = false;      // baseUrl 已配置
    bool available = false;    // 有新版本且尚未下载
    bool downloading = false;  // 下载进行中（GUI 防重入）
    bool downloaded = false;   // 制品已校验落盘
    std::string localVersion;
    UpdateManifest candidate;
    std::string downloadPath;
    int64_t lastCheckMs = 0;  // epoch 毫秒；0 = 尚未检查
    std::string lastError;

    nlohmann::json toJson() const;
};

// 状态变更回调：仅在 (available, downloaded, candidate.version, lastError)
// 组合变化时触发，避免每个轮询周期重复发事件。
using UpdateStateCallback = std::function<void(const UpdateStatus&)>;

class UpdatePoller {
public:
    explicit UpdatePoller(UpdatePollConfig cfg);
    ~UpdatePoller();

    UpdatePoller(const UpdatePoller&) = delete;
    UpdatePoller& operator=(const UpdatePoller&) = delete;

    // 启动周期轮询。重复调用无效；stop() 后可再次 start()。
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // 请求立即检查一次（worker 线程执行，不阻塞调用方）。
    void requestCheck();
    // 请求下载当前候选（worker 线程执行，不阻塞调用方）。
    void requestDownload();

    void setOnStateChanged(UpdateStateCallback cb);

    // 以下为 worker 执行的核心逻辑，公开以便单测在无网络下直接断言状态机。
    bool pollOnce();
    bool downloadCandidate();

    // 重置为初始状态（配置变更/热重建时使用）。
    void reset();

    UpdateStatus status() const;
    const UpdatePollConfig& config() const { return cfg_; }

    static int64_t nowMs();

private:
    void loop();
    void emitChange();
    std::string reportKey() const;
    void setError(const std::string& msg);

    UpdatePollConfig cfg_;
    mutable std::mutex mutex_;
    UpdateStatus status_;

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::mutex wakeMutex_;
    std::condition_variable wake_;
    std::atomic<bool> checkRequested_{false};
    std::atomic<bool> downloadRequested_{false};

    UpdateStateCallback onStateChanged_;
    std::string lastReportKey_;
};

// 把 manifest 里的 url（服务端返回相对路径，如 /api/v1/update/download/1）
// 解析成绝对地址；已是 http(s) 开头则原样返回。
std::string resolveArtifactUrl(const std::string& baseUrl, const std::string& url);

// 为候选制品选本地落盘文件名：优先 manifest.fileName，其次 url 路径末段，
// 最后回退 wingman-<version>。只取末段并剔除空/点号/路径分隔符，避免写出
// 到 downloadDir 之外。
std::string artifactFileName(const UpdateManifest& m, const std::string& fallbackVersion);

} // namespace wingman
