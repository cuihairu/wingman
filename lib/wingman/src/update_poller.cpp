#include "wingman/update_poller.hpp"

#include <chrono>
#include <cctype>

namespace wingman {

namespace {

constexpr int kMinPollIntervalSeconds = 60;

// 默认传输：绑定 update.hpp 的公开实现。
UpdateCheckFn defaultCheckFn() { return &checkUpdateLatest; }

UpdateDownloadFn defaultDownloadFn() { return &downloadUpdateArtifact; }

int clampPollInterval(int seconds) {
    return seconds < kMinPollIntervalSeconds ? kMinPollIntervalSeconds : seconds;
}

nlohmann::json manifestToJson(const UpdateManifest& m) {
    return {
        {"version", m.version},
        {"url", m.url},
        {"sha256", m.sha256},
        {"size", m.size},
        {"notes", m.notes},
        {"mandatory", m.mandatory},
        {"fileName", m.fileName},
    };
}

} // namespace

nlohmann::json UpdateStatus::toJson() const {
    return {
        {"enabled", enabled},
        {"available", available},
        {"downloading", downloading},
        {"downloaded", downloaded},
        {"localVersion", localVersion},
        {"candidate", manifestToJson(candidate)},
        {"downloadPath", downloadPath},
        {"lastCheckMs", lastCheckMs},
        {"lastError", lastError},
    };
}

int64_t UpdatePoller::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

UpdatePoller::UpdatePoller(UpdatePollConfig cfg)
    : cfg_(std::move(cfg)),
      status_() {
    status_.enabled = !cfg_.baseUrl.empty();
    status_.localVersion = cfg_.localVersion;
    if (!status_.enabled) {
        status_.lastError = "update endpoint not configured";
    }
}

void UpdatePoller::setError(const std::string& msg) {
    status_.lastError = msg;
}

void UpdatePoller::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = UpdateStatus{};
    status_.enabled = !cfg_.baseUrl.empty();
    status_.localVersion = cfg_.localVersion;
    if (!status_.enabled) {
        status_.lastError = "update endpoint not configured";
    }
    {
        std::lock_guard<std::mutex> lock(wakeMutex_);
        lastReportKey_.clear();
    }
}

bool UpdatePoller::pollOnce() {
    const auto checkFn = cfg_.checkFn ? cfg_.checkFn : defaultCheckFn();

    std::lock_guard<std::mutex> lock(mutex_);
    if (!status_.enabled) {
        setError("update endpoint not configured");
        return false;
    }

    UpdateManifest candidate;
    const int64_t startedAt = nowMs();
    const bool ok = checkFn(cfg_.baseUrl, cfg_.channel, cfg_.platform, cfg_.arch, candidate);
    status_.lastCheckMs = nowMs();
    (void)startedAt;
    if (!ok) {
        // 网络/服务不可用不丢弃已有候选，便于稍后重试下载。
        setError("check failed (network or server unavailable)");
        return false;
    }
    setError("");

    if (compareUpdateVersions(candidate.version, status_.localVersion) <= 0) {
        status_.available = false;
        status_.downloaded = false;
        status_.downloadPath.clear();
        status_.candidate = UpdateManifest{};
        return false;
    }

    const std::string previous = status_.candidate.version;
    status_.candidate = candidate;
    status_.downloaded = false;
    status_.downloadPath.clear();
    status_.available = true;
    // 仅在候选版本变化时上报，避免每个轮询周期重复发事件。
    return status_.candidate.version != previous;
}

bool UpdatePoller::downloadCandidate() {
    std::string url;
    std::string destPath;
    std::string sha256;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.downloading) {
            setError("download already in progress");
            return false;
        }
        if (!status_.candidate.version.empty() && status_.downloaded) {
            // 幂等：制品已校验落盘，保持既有 downloadPath 可见后直接返回。
            setError("");
            return true;
        }
        if (status_.candidate.version.empty()) {
            setError("no update candidate to download");
            return false;
        }
        if (cfg_.downloadDir.empty()) {
            setError("download directory not configured");
            return false;
        }
        status_.downloading = true;
        setError("");
        url = resolveArtifactUrl(cfg_.baseUrl, status_.candidate.url);
        destPath = cfg_.downloadDir + "/" +
            artifactFileName(status_.candidate, status_.candidate.version);
        sha256 = status_.candidate.sha256;
    }

    // 传输本身可能耗时数分钟，期间释放锁，让 status() 能及时返回
    // downloading=true。
    const auto downloadFn = cfg_.downloadFn ? cfg_.downloadFn : defaultDownloadFn();
    const bool ok = downloadFn(url, destPath, sha256);

    std::lock_guard<std::mutex> lock(mutex_);
    status_.downloading = false;
    if (ok) {
        status_.downloaded = true;
        status_.available = false;
        status_.downloadPath = destPath;
        setError("");
    } else {
        // 候选仍在，允许重试；不重置 available。
        status_.available = true;
        setError("download failed (transfer or sha256 mismatch)");
    }
    return ok;
}

UpdatePoller::~UpdatePoller() { stop(); }

bool UpdatePoller::start() {
    if (running_.exchange(true)) {
        return false;  // 已在运行
    }
    stopRequested_.store(false);
    worker_ = std::thread([this]() { loop(); });
    return true;
}

void UpdatePoller::stop() {
    stopRequested_.store(true);
    wake_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    running_.store(false);
}

void UpdatePoller::requestCheck() {
    if (!running_.load()) {
        return;
    }
    checkRequested_.store(true);
    wake_.notify_all();
}

void UpdatePoller::requestDownload() {
    if (!running_.load()) {
        return;
    }
    downloadRequested_.store(true);
    wake_.notify_all();
}

void UpdatePoller::setOnStateChanged(UpdateStateCallback cb) {
    std::lock_guard<std::mutex> lock(wakeMutex_);
    onStateChanged_ = std::move(cb);
}

void UpdatePoller::loop() {
    const int intervalSeconds = clampPollInterval(cfg_.pollIntervalSeconds);
    while (!stopRequested_.load()) {
        {
            std::unique_lock<std::mutex> lock(wakeMutex_);
            wake_.wait_for(lock, std::chrono::seconds(intervalSeconds), [this]() {
                return stopRequested_.load() || checkRequested_.load() ||
                    downloadRequested_.load();
            });
        }
        if (stopRequested_.load()) {
            break;
        }
        if (checkRequested_.exchange(false)) {
            pollOnce();
            emitChange();
        }
        if (downloadRequested_.exchange(false)) {
            downloadCandidate();
            emitChange();
        }
    }
}

void UpdatePoller::emitChange() {
    const std::string key = reportKey();
    UpdateStateCallback cb;
    {
        std::lock_guard<std::mutex> lock(wakeMutex_);
        if (key == lastReportKey_) {
            return;
        }
        lastReportKey_ = key;
        cb = onStateChanged_;
    }
    if (cb) {
        cb(status());
    }
}

std::string UpdatePoller::reportKey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::to_string(status_.available) + "|" + std::to_string(status_.downloaded) + "|" +
        status_.candidate.version + "|" + status_.lastError;
}

UpdateStatus UpdatePoller::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string resolveArtifactUrl(const std::string& baseUrl, const std::string& url) {
    if (url.empty()) {
        return std::string();
    }
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        return url;
    }
    std::string path = url;
    if (path[0] != '/') {
        path.insert(path.begin(), '/');
    }
    std::string base = baseUrl;
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    if (base.empty()) {
        return path;
    }
    return base + path;
}

std::string artifactFileName(const UpdateManifest& m, const std::string& fallbackVersion) {
    // 只取路径末段：fileName 由服务端上传时的 multipart 名而来，url 是服务端
    // 生成的相对路径，两者都不应带目录分量。
    auto basenameOf = [](const std::string& raw) {
        std::string s = raw;
        for (auto& ch : s) {
            if (ch == '\\') {
                ch = '/';
            }
        }
        const std::size_t slash = s.find_last_of('/');
        std::string name = (slash == std::string::npos) ? s : s.substr(slash + 1);
        std::string out;
        out.reserve(name.size());
        for (char ch : name) {
            if (ch == '/' || ch == '\\') {
                continue;
            }
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return out;
    };

    std::string name = basenameOf(m.fileName);
    if (name.empty()) {
        name = basenameOf(m.url);
        // 服务端生成的下载 url 末段是发布 ID 数字，没有扩展名，不可用。
        if (!name.empty() && name.find_first_not_of("0123456789") == std::string::npos) {
            name.clear();
        }
    }
    if (name.empty()) {
        name = "wingman-" + fallbackVersion;
    }
    if (name.size() > 200) {
        name.resize(200);
    }
    return name;
}

} // namespace wingman
