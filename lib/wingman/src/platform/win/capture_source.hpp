#pragma once

// Windows 平台的捕获源实现（ScreenCaptureSource / WindowCaptureSource /
// CaptureSourceManager）。从公共 include/wingman/capture/capture_source.hpp
// 收回（P2），公共侧只保留 ICaptureSource 接口。
// 薄层纪律见 docs/platform-abstraction-design.md §8。

#include "wingman/capture/capture_source.hpp"
#include "wingman/core/component.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wingman::capture {

/**
 * @brief Screen capture source
 *
 * Captures the entire display or a specified monitor
 */
class ScreenCaptureSource : public core::ComponentBase, public ICaptureSource {
public:
    explicit ScreenCaptureSource(int monitorIndex = 0);

    ~ScreenCaptureSource() override = default;

    // ICaptureSource interface
    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    int getMonitorIndex() const { return monitorIndex_; }

    static int getMonitorCount();

    static std::shared_ptr<ScreenCaptureSource> getPrimaryScreen();

protected:
    bool onInitialize() override;
    bool onStart() override;
    void onStop() override;

private:
    int monitorIndex_ = 0;
    Rect bounds_;
    bool available_ = false;

    bool queryDisplayInfo();
};

/**
 * @brief Window capture source
 *
 * Captures the content of a specific window
 */
class WindowCaptureSource : public core::ComponentBase, public ICaptureSource {
public:
    explicit WindowCaptureSource(HWND hwnd);

    ~WindowCaptureSource() override = default;

    // ICaptureSource interface
    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    HWND getHwnd() const { return hwnd_; }

    const std::string& getWindowTitle() const { return windowTitle_; }

    static std::unique_ptr<WindowCaptureSource> findByTitle(const std::string& title);

    static std::unique_ptr<WindowCaptureSource> findByClassName(const std::string& className);

    static std::vector<HWND> listTopLevelWindows();

protected:
    bool onInitialize() override;
    bool onStart() override;
    void onStop() override;

private:
    HWND hwnd_ = nullptr;
    std::string windowTitle_;
    Rect bounds_;
    bool available_ = false;

    bool queryWindowInfo();
    static std::string getWindowTitle(HWND hwnd);
};

/**
 * @brief Capture source manager
 *
 * Manages all registered capture sources
 */
class CaptureSourceManager {
public:
    static CaptureSourceManager& instance();

    CaptureSourceManager(const CaptureSourceManager&) = delete;
    CaptureSourceManager& operator=(const CaptureSourceManager&) = delete;

    void registerSource(std::shared_ptr<ICaptureSource> source);

    void removeSource(const std::string& name);

    std::shared_ptr<ICaptureSource> getSource(const std::string& name) const;

    std::shared_ptr<ScreenCaptureSource> getPrimaryScreen();

    std::shared_ptr<ScreenCaptureSource> getScreenSource(const std::string& name);

    std::shared_ptr<WindowCaptureSource> getWindowSource(const std::string& name);

    std::vector<std::shared_ptr<ICaptureSource>> listSources() const;

    template<typename T>
    std::vector<std::shared_ptr<T>> listSourcesByType() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<T>> result;
        for (const auto& [name, source] : sources_) {
            auto typed = std::dynamic_pointer_cast<T>(source);
            if (typed) {
                result.push_back(typed);
            }
        }
        return result;
    }

    void clear();

    size_t getSourceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sources_.size();
    }

private:
    CaptureSourceManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ICaptureSource>> sources_;

    std::string generateName(const std::string& prefix);
    uint32_t nextId_ = 1;
};

} // namespace wingman::capture
