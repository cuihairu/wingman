#pragma once

#include "wingman/platform/iscreen.hpp"

#include <memory>

namespace wingman::platform {

/**
 * @brief Create the platform-native IScreen implementation
 *
 * Each platform compiles exactly one implementation of this function
 * (win32_screen.cpp / cocoa_screen.cpp / x11_screen.cpp). The returned
 * IScreen is initialized and ready for use.
 *
 * Used by callers that need display enumeration / per-monitor bounds
 * (e.g. screenshot handler resolves `displayId` to a Rect through it).
 * The legacy static `wingman::Screen` capture API remains the capture
 * implementation; this factory only provides display metadata.
 */
std::unique_ptr<IScreen> createPlatformScreen();

} // namespace wingman::platform
