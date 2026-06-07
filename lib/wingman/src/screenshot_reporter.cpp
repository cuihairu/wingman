#include "wingman/screenshot_reporter.hpp"

#ifdef _WIN32

#include <algorithm>
#include <chrono>
#include <exception>
#include <vector>

#include <spdlog/spdlog.h>

#if defined(WINGMAN_HAS_CURL) && defined(WINGMAN_ENABLE_VISION)
#include "wingman/screen.hpp"
#include <curl/curl.h>
#include <opencv2/opencv.hpp>
#include <sstream>
#endif

namespace wingman {

namespace {

#if defined(WINGMAN_HAS_CURL) && defined(WINGMAN_ENABLE_VISION)

void ensureCurlGlobalInit() {
    static const bool initialized = []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)initialized;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

#endif

} // namespace

ScreenshotReporter::ScreenshotReporter(const ScreenshotReporterConfig& config)
    : config_(config) {
#if defined(WINGMAN_HAS_CURL) && defined(WINGMAN_ENABLE_VISION)
    ensureCurlGlobalInit();
#endif
}

ScreenshotReporter::~ScreenshotReporter() {
    stop();
}

void ScreenshotReporter::start() {
#if !defined(WINGMAN_HAS_CURL)
    spdlog::warn("ScreenshotReporter disabled: CURL support is not enabled");
    return;
#elif !defined(WINGMAN_ENABLE_VISION)
    spdlog::warn("ScreenshotReporter disabled: vision support is not enabled");
    return;
#else
    if (running_) {
        spdlog::warn("ScreenshotReporter already running");
        return;
    }
    running_ = true;
    worker_ = std::thread(&ScreenshotReporter::workerThread, this);
    spdlog::info("ScreenshotReporter started, interval: {}ms", config_.intervalMs);
#endif
}

void ScreenshotReporter::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    spdlog::info("ScreenshotReporter stopped");
}

void ScreenshotReporter::updateConfig(const ScreenshotReporterConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

void ScreenshotReporter::setCallback(ScreenshotCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = std::move(callback);
}

bool ScreenshotReporter::captureAndReport() {
    return captureAndSend();
}

void ScreenshotReporter::workerThread() {
#if defined(WINGMAN_HAS_CURL) && defined(WINGMAN_ENABLE_VISION)
    while (running_) {
        bool enabled = false;
        int intervalMs = 100;
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            enabled = config_.enabled;
            intervalMs = enabled ? std::max(1, config_.intervalMs) : 100;
        }

        if (enabled) {
            captureAndSend();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
#endif
}

bool ScreenshotReporter::captureAndSend() {
#if !defined(WINGMAN_HAS_CURL)
    spdlog::warn("ScreenshotReporter unavailable: CURL support is not enabled");
    return false;
#elif !defined(WINGMAN_ENABLE_VISION)
    spdlog::warn("ScreenshotReporter unavailable: vision support is not enabled");
    return false;
#else
    ScreenshotReporterConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config = config_;
    }

    try {
        auto bitmap = Screen::capture();
        if (!bitmap) {
            spdlog::error("ScreenshotReporter: failed to capture screen");
            return false;
        }

        int width = bitmap->getWidth();
        int height = bitmap->getHeight();

        cv::Mat bgraMat(height, width, CV_8UC4, bitmap->getData());
        if (config.scaleWidth > 0 && config.scaleHeight > 0) {
            cv::resize(bgraMat, bgraMat, cv::Size(config.scaleWidth, config.scaleHeight));
            width = config.scaleWidth;
            height = config.scaleHeight;
        }

        cv::Mat bgrMat;
        cv::cvtColor(bgraMat, bgrMat, cv::COLOR_BGRA2BGR);

        std::vector<uchar> jpegBuffer;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, config.quality};
        cv::imencode(".jpg", bgrMat, jpegBuffer, params);

        const std::string base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string base64;
        base64.reserve((jpegBuffer.size() * 4) / 3);

        for (size_t i = 0; i < jpegBuffer.size(); i += 3) {
            const auto b0 = jpegBuffer[i];
            const auto b1 = (i + 1 < jpegBuffer.size()) ? jpegBuffer[i + 1] : 0;
            const auto b2 = (i + 2 < jpegBuffer.size()) ? jpegBuffer[i + 2] : 0;

            base64.push_back(base64Chars[(b0 >> 2) & 0x3F]);
            base64.push_back(base64Chars[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
            base64.push_back((i + 1 < jpegBuffer.size()) ? base64Chars[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=');
            base64.push_back((i + 2 < jpegBuffer.size()) ? base64Chars[b2 & 0x3F] : '=');
        }

        // Invoke callback with mutex protection
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (callback_) {
                callback_(base64, width, height);
            }
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            spdlog::error("ScreenshotReporter: failed to init CURL");
            return false;
        }

        std::stringstream json;
        json << "{\"image\":\"data:image/jpeg;base64," << base64
             << "\",\"width\":" << width
             << ",\"height\":" << height
             << ",\"timestamp\":"
             << std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()
             << "}";

        const std::string jsonData = json.str();
        const std::string url = config.serverUrl + "/api/v1/screenshot";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonData.length()));

        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        const CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && httpCode == 200) {
            spdlog::trace("ScreenshotReporter: uploaded successfully");
            lastCaptureTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("ScreenshotReporter: exception: {}", e.what());
    }

    return false;
#endif
}

} // namespace wingman

#endif // _WIN32
