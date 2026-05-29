#pragma once

#include "wingman/platform/platform_types.hpp"
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief Window management interface
 *
 * Provides window finding, operations, property query, etc.
 */
class IWindow {
public:
    virtual ~IWindow() = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize window manager
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown window manager
     */
    virtual void shutdown() = 0;

    // ========== Find window ==========

    /**
     * @brief Find window by title (returns first match)
     * @param title Window title (supports partial match)
     */
    virtual WindowHandle find(const std::string& title) = 0;

    /**
     * @brief Find all matching windows by title
     * @param title Window title (supports partial match)
     */
    virtual std::vector<WindowHandle> findAll(const std::string& title) = 0;

    /**
     * @brief Find window by class name
     * @param className Window class name
     */
    virtual WindowHandle findByClassName(const std::string& className) = 0;

    /**
     * @brief Find window by process ID
     * @param processId Process ID
     */
    virtual std::vector<WindowHandle> findByProcessId(uint32_t processId) = 0;

    /**
     * @brief Enumerate all top-level windows
     */
    virtual std::vector<WindowInfo> enumerate() = 0;

    /**
     * @brief Get current foreground window
     */
    virtual WindowHandle getForeground() = 0;

    // ========== Window operations ==========

    /**
     * @brief Activate window (bring to top and focus)
     * @param hwnd Window handle
     */
    virtual bool activate(WindowHandle hwnd) = 0;

    /**
     * @brief Minimize window
     * @param hwnd Window handle
     */
    virtual bool minimize(WindowHandle hwnd) = 0;

    /**
     * @brief Maximize window
     * @param hwnd Window handle
     */
    virtual bool maximize(WindowHandle hwnd) = 0;

    /**
     * @brief Restore window
     * @param hwnd Window handle
     */
    virtual bool restore(WindowHandle hwnd) = 0;

    /**
     * @brief Close window (send close message)
     * @param hwnd Window handle
     */
    virtual bool close(WindowHandle hwnd) = 0;

    /**
     * @brief Force close window
     * @param hwnd Window handle
     */
    virtual bool forceClose(WindowHandle hwnd) = 0;

    /**
     * @brief Hide window
     * @param hwnd Window handle
     */
    virtual bool hide(WindowHandle hwnd) = 0;

    /**
     * @brief Show window
     * @param hwnd Window handle
     */
    virtual bool show(WindowHandle hwnd) = 0;

    // ========== Window properties ==========

    /**
     * @brief Get window title
     * @param hwnd Window handle
     */
    virtual std::string getTitle(WindowHandle hwnd) = 0;

    /**
     * @brief Get window bounds
     * @param hwnd Window handle
     */
    virtual Rect getBounds(WindowHandle hwnd) = 0;

    /**
     * @brief Set window bounds
     * @param hwnd Window handle
     * @param bounds New bounds
     */
    virtual bool setBounds(WindowHandle hwnd, const Rect& bounds) = 0;

    /**
     * @brief Move window
     * @param hwnd Window handle
     * @param x New X coordinate
     * @param y New Y coordinate
     */
    virtual bool move(WindowHandle hwnd, int x, int y) = 0;

    /**
     * @brief Resize window
     * @param hwnd Window handle
     * @param width New width
     * @param height New height
     */
    virtual bool resize(WindowHandle hwnd, int width, int height) = 0;

    /**
     * @brief Center window on specified monitor
     * @param hwnd Window handle
     * @param monitorIndex Monitor index
     */
    virtual bool center(WindowHandle hwnd, int monitorIndex) = 0;

    // ========== Window state ==========

    /**
     * @brief Check if window handle is valid
     * @param hwnd Window handle
     */
    virtual bool isValid(WindowHandle hwnd) = 0;

    /**
     * @brief Check if window is visible
     * @param hwnd Window handle
     */
    virtual bool isVisible(WindowHandle hwnd) = 0;

    /**
     * @brief Check if window is foreground window
     * @param hwnd Window handle
     */
    virtual bool isForeground(WindowHandle hwnd) = 0;

    /**
     * @brief Check if window is minimized
     * @param hwnd Window handle
     */
    virtual bool isMinimized(WindowHandle hwnd) = 0;

    /**
     * @brief Check if window is maximized
     * @param hwnd Window handle
     */
    virtual bool isMaximized(WindowHandle hwnd) = 0;

    /**
     * @brief Get window process ID
     * @param hwnd Window handle
     */
    virtual std::optional<uint32_t> getProcessId(WindowHandle hwnd) = 0;

    // ========== Wait operations ==========

    /**
     * @brief Wait for window to appear
     * @param title Window title
     * @param timeoutMs Timeout (milliseconds)
     */
    virtual bool waitFor(const std::string& title, int timeoutMs) = 0;

    /**
     * @brief Wait for window to close
     * @param title Window title
     * @param timeoutMs Timeout (milliseconds)
     */
    virtual bool waitClose(const std::string& title, int timeoutMs) = 0;

    /**
     * @brief Wait for window to become foreground
     * @param hwnd Window handle
     * @param timeoutMs Timeout (milliseconds)
     */
    virtual bool waitForForeground(WindowHandle hwnd, int timeoutMs) = 0;

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
