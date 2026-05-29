#include "wingman/platform/icapture.hpp"
#include "wingman/screen.hpp"
#include <map>
#include <mutex>

namespace wingman::platform::mock {

/**
 * @brief Mock capture implementation (for testing)
 */
class MockCapture : public ICapture {
public:
    MockCapture() = default;
    ~MockCapture() override {
        shutdown();
    }

    bool initialize(const CaptureConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = false;
    }

    std::unique_ptr<Bitmap> captureScreen(int monitorIndex) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            return nullptr;
        }

        Rect bounds = getMonitorBounds(monitorIndex);
        return createTestBitmap(bounds.width, bounds.height);
    }

    std::unique_ptr<Bitmap> captureRegion(const Rect& region) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            return nullptr;
        }
        return createTestBitmap(region.width, region.height);
    }

    std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) override {
        return captureRegion(region);
    }

    std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            return nullptr;
        }

        auto it = windowBounds_.find(hwnd);
        if (it != windowBounds_.end()) {
            return createTestBitmap(it->second.width, it->second.height);
        }
        return createTestBitmap(800, 600);
    }

    std::unique_ptr<Bitmap> captureWindowByTitle(const std::string& title) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            return nullptr;
        }
        return createTestBitmap(800, 600);
    }

    int getMonitorCount() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return mockMonitorCount_;
    }

    Rect getMonitorBounds(int monitorIndex) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (monitorIndex == 0) {
            return Rect{0, 0, 1920, 1080};
        }
        return Rect{1920, 0, 1920, 1080};
    }

    std::string getMonitorName(int monitorIndex) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return "Mock Monitor " + std::to_string(monitorIndex);
    }

    int getPrimaryMonitorIndex() override {
        return 0;
    }

    double getDpiScale(int monitorIndex) override {
        return 1.0;
    }

    std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) override {
        (void)monitorIndex;
        return {
            DisplayMode{1920, 1080, 60, 32},
            DisplayMode{2560, 1440, 60, 32},
            DisplayMode{3840, 2160, 30, 32}
        };
    }

    std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) override {
        (void)monitorIndex;
        return DisplayMode{1920, 1080, 60, 32};
    }

    bool isAvailable() const override {
        return initialized_;
    }

    std::string getBackendName() const override {
        return "Mock";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Mock",
            "1.0",
            initialized_,
            "Mock Capture for Testing"
        };
    }

    bool supportsWindowCapture() const override {
        return true;
    }

    bool supportsCursorCapture() const override {
        return false;
    }

    CaptureConfig getConfig() const override {
        return config_;
    }

    bool updateConfig(const CaptureConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        return true;
    }

    // ========== Mock-Specific Methods ==========

    void setMockMonitorCount(int count) {
        std::lock_guard<std::mutex> lock(mutex_);
        mockMonitorCount_ = count;
    }

    void setMockWindowBounds(WindowHandle hwnd, const Rect& bounds) {
        std::lock_guard<std::mutex> lock(mutex_);
        windowBounds_[hwnd] = bounds;
    }

    void clearMockWindows() {
        std::lock_guard<std::mutex> lock(mutex_);
        windowBounds_.clear();
    }

private:
    CaptureConfig config_;
    bool initialized_ = false;
    int mockMonitorCount_ = 1;
    std::map<WindowHandle, Rect> windowBounds_;
    mutable std::mutex mutex_;

    static std::unique_ptr<Bitmap> createTestBitmap(int width, int height) {
        auto bitmap = std::make_unique<Bitmap>(width, height);

        // Fill test pattern
        uint8_t* data = bitmap->getData();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 4;
                // Create gradient pattern
                data[index + 0] = static_cast<uint8_t>((x * 255) / width);     // B
                data[index + 1] = static_cast<uint8_t>((y * 255) / height);    // G
                data[index + 2] = 128;                                        // R
                data[index + 3] = 255;                                        // A
            }
        }

        return bitmap;
    }
};

} // namespace wingman::platform::mock
