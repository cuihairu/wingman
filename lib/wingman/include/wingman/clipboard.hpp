#pragma once

#include "wingman/platform/iclipboard.hpp"
#include <memory>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

namespace wingman {

// Forward declaration
namespace platform {
    class IClipboard;
}

/**
 * @brief Clipboard management class
 *
 * Provides cross-platform clipboard access interface, automatically using the best platform implementation.
 */
class Clipboard {
public:
    /**
     * @brief Get clipboard singleton
     */
    static platform::IClipboard& instance();

    // ========== Convenience static methods ==========

    /**
     * @brief Set clipboard text
     */
    static bool setText(const std::string& text);

    /**
     * @brief Get clipboard text
     */
    static std::string getText();

    /**
     * @brief Check if clipboard contains text
     */
    static bool hasText();

    /**
     * @brief Set clipboard HTML
     */
    static bool setHTML(const std::string& html);

    /**
     * @brief Get clipboard HTML
     */
    static std::string getHTML();

    /**
     * @brief Check if clipboard contains HTML
     */
    static bool hasHTML();

    /**
     * @brief Set clipboard image
     */
    static bool setImage(const std::vector<uint8_t>& imageData, int width, int height);

    /**
     * @brief Get clipboard image
     */
    static std::vector<uint8_t> getImage(int* outWidth, int* outHeight);

    /**
     * @brief Check if clipboard contains image
     */
    static bool hasImage();

    /**
     * @brief Set clipboard file list
     */
    static bool setFiles(const std::vector<std::string>& files);

    /**
     * @brief Get clipboard file list
     */
    static std::vector<std::string> getFiles();

    /**
     * @brief Check if clipboard contains files
     */
    static bool hasFiles();

    /**
     * @brief Clear clipboard
     */
    static void clear();

    /**
     * @brief Check if clipboard is empty
     */
    static bool isEmpty();
};

} // namespace wingman
