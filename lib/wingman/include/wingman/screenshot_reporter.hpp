#pragma once

#ifdef _WIN32
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace wingman {

// 截图上报配置
struct ScreenshotReporterConfig {
    std::string serverUrl = "http://localhost:9527";
    int intervalMs = 1000;
    int quality = 75;
    int scaleWidth = 0;
    int scaleHeight = 0;
    bool enabled = true;
};

using ScreenshotCallback = std::function<void(const std::string& base64Image, int width, int height)>;

class ScreenshotReporter {
public:
    explicit ScreenshotReporter(const ScreenshotReporterConfig& config);
    ~ScreenshotReporter();

    void start();
    void stop();
    bool isRunning() const { return running_; }
    void updateConfig(const ScreenshotReporterConfig& config);
    void setCallback(ScreenshotCallback callback) { callback_ = std::move(callback); }
    bool captureAndReport();
    int64_t getLastCaptureTime() const { return lastCaptureTime_; }

private:
    void workerThread();
    bool captureAndSend();

    ScreenshotReporterConfig config_;
    std::mutex configMutex_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::atomic<int64_t> lastCaptureTime_{0};
    ScreenshotCallback callback_;
};

} // namespace wingman

#endif // _WIN32
