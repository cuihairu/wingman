#include "wingman/screenshot_reporter.hpp"
#include "wingman/screen.hpp"
#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>

namespace wingman {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

ScreenshotReporter::ScreenshotReporter(const ScreenshotReporterConfig& config)
    : config_(config)
{
    static bool curlInitialized = false;
    if (!curlInitialized) {
        curl_global_init(CURL_GLOBAL_ALL);
        curlInitialized = true;
    }
}

ScreenshotReporter::~ScreenshotReporter() {
    stop();
}

void ScreenshotReporter::start() {
    if (running_) {
        spdlog::warn("ScreenshotReporter already running");
        return;
    }
    running_ = true;
    worker_ = std::thread(&ScreenshotReporter::workerThread, this);
    spdlog::info("ScreenshotReporter started, interval: {}ms", config_.intervalMs);
}

void ScreenshotReporter::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_.joinable()) worker_.join();
    spdlog::info("ScreenshotReporter stopped");
}

void ScreenshotReporter::updateConfig(const ScreenshotReporterConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

bool ScreenshotReporter::captureAndReport() {
    return captureAndSend();
}

void ScreenshotReporter::workerThread() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            if (!config_.enabled) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }
        captureAndSend();
        std::lock_guard<std::mutex> lock(configMutex_);
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.intervalMs));
    }
}

bool ScreenshotReporter::captureAndSend() {
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
            auto b0 = jpegBuffer[i];
            auto b1 = (i + 1 < jpegBuffer.size()) ? jpegBuffer[i + 1] : 0;
            auto b2 = (i + 2 < jpegBuffer.size()) ? jpegBuffer[i + 2] : 0;

            base64.push_back(base64Chars[(b0 >> 2) & 0x3F]);
            base64.push_back(base64Chars[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
            base64.push_back((i + 1 < jpegBuffer.size()) ? base64Chars[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=');
            base64.push_back((i + 2 < jpegBuffer.size()) ? base64Chars[b2 & 0x3F] : '=');
        }

        if (callback_) {
            callback_(base64, width, height);
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
             << ",\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() << "}";

        std::string jsonData = json.str();
        std::string url = config.serverUrl + "/api/v1/screenshot";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonData.length());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (httpCode == 200) {
                spdlog::trace("ScreenshotReporter: uploaded successfully");
                lastCaptureTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                return true;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("ScreenshotReporter: exception: {}", e.what());
    }
    return false;
}

} // namespace wingman
