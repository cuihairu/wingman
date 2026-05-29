#pragma once

#include "wingman/platform/platform_types.hpp"
#include <memory>
#include <string>

// Forward declaration of Bitmap
namespace wingman {
class Bitmap;
}

namespace wingman::platform {

/**
 * @brief Capture configuration
 */
struct CaptureConfig {
    CaptureBackend preferredBackend = CaptureBackend::Auto;
    int monitorIndex = 0;
    int fps = 60;
    bool includeCursor = false;
    bool useHardwareAcceleration = true;
};

/**
 * @brief Screen capture interface
 *
 * Provides screen, window, region image capture functionality.
 */
class ICapture {
public:
    virtual ~ICapture() = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize capturer
     * @param config Configuration
     */
    virtual bool initialize(const CaptureConfig& config) = 0;

    /**
     * @brief Shutdown capturer
     */
    virtual void shutdown() = 0;

    // ========== Screen capture ==========

    /**
     * @brief Capture entire screen
     * @param monitorIndex Monitor index
     */
    virtual std::unique_ptr<Bitmap> captureScreen(int monitorIndex) = 0;

    /**
     * @brief Capture screen region
     * @param region Region
     */
    virtual std::unique_ptr<Bitmap> captureRegion(const Rect& region) = 0;

    /**
     * @brief Capture specified monitor region
     * @param monitorIndex Monitor index
     * @param region Region
     */
    virtual std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) = 0;

    // ========== Window capture ==========

    /**
     * @brief Capture window
     * @param hwnd Window handle
     */
    virtual std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) = 0;

    /**
     * @brief Capture window region
     * @param hwnd Window handle
     * @param region Region relative to window
     */
    virtual std::unique_ptr<Bitmap> captureWindowRegion(WindowHandle hwnd, const Rect& region) = 0;

    // ========== Information query ==========

    /**
     * @brief Get monitor count
     */
    virtual int getMonitorCount() = 0;

    /**
     * @brief Get monitor bounds
     * @param monitorIndex Monitor index
     */
    virtual Rect getMonitorBounds(int monitorIndex) = 0;

    /**
     * @brief Get monitor name
     * @param monitorIndex Monitor index
     */
    virtual std::string getMonitorName(int monitorIndex) = 0;

    // ========== Status ==========

    /**
     * @brief Check if capturer is available
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Get backend name
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief Get backend info
     */
    virtual BackendInfo getBackendInfo() const = 0;
};

} // namespace wingman::platform
