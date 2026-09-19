#pragma once

// Bitmap/Rect 完整类型目前定义于遗留 wingman/screen.hpp（P4 下线时随
// wingman::platform 类型体系统一），本头本身保持零平台宏。
#include "wingman/screen.hpp"
#include <memory>
#include <string>

namespace wingman::capture {

/**
 * @brief Capture source abstract interface
 *
 * Supports different capture sources: screen, window, camera, etc.
 * Platform implementations live in lib/wingman/src/platform/<os>/
 * (thin-layer discipline, docs/platform-abstraction-design.md §8).
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

} // namespace wingman::capture
