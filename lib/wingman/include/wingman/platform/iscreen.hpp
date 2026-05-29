#pragma once

#include "wingman/platform/platform_types.hpp"
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief Screen management interface
 *
 * Provides display info query, DPI scaling, display mode settings, etc.
 */
class IScreen {
public:
    virtual ~IScreen() = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize screen manager
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown screen manager
     */
    virtual void shutdown() = 0;

    // ========== Monitor information ==========

    /**
     * @brief Get monitor count
     */
    virtual int getMonitorCount() = 0;

    /**
     * @brief Get primary monitor index
     */
    virtual int getPrimaryMonitorIndex() = 0;

    /**
     * @brief Get primary monitor bounds
     */
    virtual Rect getPrimaryMonitorBounds() = 0;

    /**
     * @brief Get specified monitor bounds
     * @param monitorIndex Monitor index
     */
    virtual Rect getMonitorBounds(int monitorIndex) = 0;

    /**
     * @brief Get specified monitor work area bounds (excluding taskbar)
     * @param monitorIndex Monitor index
     */
    virtual Rect getMonitorWorkArea(int monitorIndex) = 0;

    /**
     * @brief Get monitor name
     * @param monitorIndex Monitor index
     */
    virtual std::string getMonitorName(int monitorIndex) = 0;

    /**
     * @brief Check if primary monitor
     * @param monitorIndex Monitor index
     */
    virtual bool isPrimaryMonitor(int monitorIndex) = 0;

    // ========== DPI scaling ==========

    /**
     * @brief Get DPI scaling ratio
     * @param monitorIndex Monitor index
     * @return Scaling ratio (1.0 = 96 DPI)
     */
    virtual double getDpiScale(int monitorIndex) = 0;

    /**
     * @brief Get DPI value
     * @param monitorIndex Monitor index
     */
    virtual int getDpi(int monitorIndex) = 0;

    /**
     * @brief Convert logical coordinates to physical coordinates
     * @param point Logical coordinates
     * @param monitorIndex Monitor index
     */
    virtual Point logicalToPhysical(const Point& point, int monitorIndex) = 0;

    /**
     * @brief Convert physical coordinates to logical coordinates
     * @param point Physical coordinates
     * @param monitorIndex Monitor index
     */
    virtual Point physicalToLogical(const Point& point, int monitorIndex) = 0;

    // ========== Display mode ==========

    /**
     * @brief Get supported display mode list
     * @param monitorIndex Monitor index
     */
    virtual std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) = 0;

    /**
     * @brief Get current display mode
     * @param monitorIndex Monitor index
     */
    virtual std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) = 0;

    /**
     * @brief Set display mode
     * @param monitorIndex Monitor index
     * @param mode Display mode
     */
    virtual bool setDisplayMode(int monitorIndex, const DisplayMode& mode) = 0;

    /**
     * @brief Reset display mode to default
     * @param monitorIndex Monitor index
     */
    virtual bool resetDisplayMode(int monitorIndex) = 0;

    // ========== Screen state ==========

    /**
     * @brief Check if screen saver is running
     */
    virtual bool isScreenSaverRunning() = 0;

    /**
     * @brief Start screen saver
     */
    virtual void startScreenSaver() = 0;

    /**
     * @brief Check if monitor is off
     */
    virtual bool isMonitorOff() = 0;

    /**
     * @brief Wake up monitor
     */
    virtual void wakeUpMonitor() = 0;

    // ========== Virtual screen ==========

    /**
     * @brief Get virtual screen bounds (area composed of all monitors)
     */
    virtual Rect getVirtualScreenBounds() = 0;

    /**
     * @brief Get monitor index containing the specified point
     * @param point Point coordinates
     */
    virtual int getMonitorFromPoint(const Point& point) = 0;

    /**
     * @brief Get monitor index containing the specified window
     * @param hwnd Window handle
     */
    virtual int getMonitorFromWindow(WindowHandle hwnd) = 0;

    // ========== Backend information ==========

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
