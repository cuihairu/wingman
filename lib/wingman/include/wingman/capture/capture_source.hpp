#pragma once

#include "wingman/screen.hpp"
#include "wingman/core/component.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace wingman::capture {

/**
 * @brief Capture source abstract interface
 *
 * Supports different capture sources: screen, window, camera, etc.
 */
class ICaptureSource {
public:
    virtual ~ICaptureSource() = default;

    /**
     * @brief Capture frame
     * @param region Capture region (empty captures all)
     * @return Captured bitmap
     */
    virtual std::unique_ptr<Bitmap> capture(const Rect& region = {}) = 0;

    /**
     * @brief Get bounds
     * @return Capture source bounding rectangle
     */
    virtual Rect getBounds() const = 0;

    /**
     * @brief Check availability
     * @return True if available
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Get name
     * @return Capture source name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get width
     */
    virtual int getWidth() const { return getBounds().width; }

    /**
     * @brief Get height
     */
    virtual int getHeight() const { return getBounds().height; }
};

/**
 * @brief Screen capture source
 *
 * Captures the entire display or a specified monitor
 */
class ScreenCaptureSource : public core::ComponentBase, public ICaptureSource {
public:
    /**
     * @brief Constructor
     * @param monitorIndex Monitor index (0 is primary monitor)
     */
    explicit ScreenCaptureSource(int monitorIndex = 0);

    ~ScreenCaptureSource() override = default;

    // ICaptureSource interface
    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    /**
     * @brief Get monitor index
     */
    int getMonitorIndex() const { return monitorIndex_; }

    /**
     * @brief Get monitor count
     */
    static int getMonitorCount();

    /**
     * @brief Get primary screen capture source
     */
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

#ifdef _WIN32
/**
 * @brief Window capture source
 *
 * Captures the content of a specific window
 */
class WindowCaptureSource : public core::ComponentBase, public ICaptureSource {
public:
    /**
     * @brief Constructor
     * @param hwnd Window handle
     */
    explicit WindowCaptureSource(HWND hwnd);

    ~WindowCaptureSource() override = default;

    // ICaptureSource interface
    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    /**
     * @brief Get window handle
     */
    HWND getHwnd() const { return hwnd_; }

    /**
     * @brief Get window title
     */
    const std::string& getWindowTitle() const { return windowTitle_; }

    /**
     * @brief Find window by title
     * @param title Window title (supports partial match)
     * @return Window capture source, nullptr if not found
     */
    static std::unique_ptr<WindowCaptureSource> findByTitle(const std::string& title);

    /**
     * @brief Find window by class name
     * @param className Window class name
     * @return Window capture source, nullptr if not found
     */
    static std::unique_ptr<WindowCaptureSource> findByClassName(const std::string& className);

    /**
     * @brief List all top-level windows
     * @return Window handle list
     */
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
#endif // _WIN32

/**
 * @brief Capture source manager
 *
 * Manages all registered capture sources
 */
class CaptureSourceManager {
public:
    static CaptureSourceManager& instance();

    // Non-copyable
    CaptureSourceManager(const CaptureSourceManager&) = delete;
    CaptureSourceManager& operator=(const CaptureSourceManager&) = delete;

    /**
     * @brief Register capture source
     * @param source Capture source
     */
    void registerSource(std::shared_ptr<ICaptureSource> source);

    /**
     * @brief Remove capture source
     * @param name Capture source name
     */
    void removeSource(const std::string& name);

    /**
     * @brief Get capture source
     * @param name Capture source name
     * @return Capture source pointer, nullptr if not found
     */
    std::shared_ptr<ICaptureSource> getSource(const std::string& name) const;

    /**
     * @brief Get default screen capture source
     * @return Primary monitor capture source
     */
    std::shared_ptr<ScreenCaptureSource> getPrimaryScreen();

    /**
     * @brief Get screen capture source by name
     */
    std::shared_ptr<ScreenCaptureSource> getScreenSource(const std::string& name);

#ifdef _WIN32
    /**
     * @brief Get window capture source by name
     */
    std::shared_ptr<WindowCaptureSource> getWindowSource(const std::string& name);
#endif // _WIN32

    /**
     * @brief List all capture sources
     * @return Capture source list
     */
    std::vector<std::shared_ptr<ICaptureSource>> listSources() const;

    /**
     * @brief List capture sources by type
     */
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

    /**
     * @brief Clear all capture sources
     */
    void clear();

    /**
     * @brief Get capture source count
     */
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
