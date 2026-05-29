#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace wingman::platform {

/**
 * @brief Clipboard format
 */
enum class ClipboardFormat : uint32_t {
    Text,           // Plain text (CF_TEXT)
    UnicodeText,    // Unicode text (CF_UNICODETEXT)
    HTML,           // HTML format
    Image,          // Image (DIB/Bitmap)
    Files,          // File list (CF_HDROP)
    Custom          // Custom format
};

/**
 * @brief Clipboard data
 */
struct ClipboardData {
    ClipboardFormat format = ClipboardFormat::Text;
    std::vector<uint8_t> data;
    std::string text;           // Text content (convenient access)
    std::string customFormat;   // Custom format name

    ClipboardData() = default;
    explicit ClipboardData(const std::string& text)
        : format(ClipboardFormat::UnicodeText), text(text) {
        data.assign(text.begin(), text.end());
    }
};

/**
 * @brief Clipboard interface
 *
 * Provides clipboard read/write functionality, supports text, HTML, image, file and other formats.
 */
class IClipboard {
public:
    virtual ~IClipboard() = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize clipboard manager
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown clipboard manager
     */
    virtual void shutdown() = 0;

    // ========== Text operations ==========

    /**
     * @brief Set clipboard text
     * @param text Text content
     * @return Returns true on success
     */
    virtual bool setText(const std::string& text) = 0;

    /**
     * @brief Get clipboard text
     * @return Text content，Returns empty string on failure
     */
    virtual std::string getText() = 0;

    /**
     * @brief Check if clipboard contains text
     * @return Returns true if text is present
     */
    virtual bool hasText() = 0;

    // ========== HTML operations ==========

    /**
     * @brief Set clipboard HTML content
     * @param html HTML content
     * @return Returns true on success
     */
    virtual bool setHTML(const std::string& html) = 0;

    /**
     * @brief Get clipboard HTML content
     * @return HTML content, returns empty string on failure
     */
    virtual std::string getHTML() = 0;

    /**
     * @brief Check if clipboard contains HTML
     * @return Returns true if HTML is present
     */
    virtual bool hasHTML() = 0;

    // ========== Image operations ==========

    /**
     * @brief Set clipboard image
     * @param imageData Image data (BGRA format, 32-bit)
     * @param width Image width
     * @param height Image height
     * @return Returns true on success
     */
    virtual bool setImage(const std::vector<uint8_t>& imageData, int width, int height) = 0;

    /**
     * @brief Get clipboard image
     * @param outWidth Output image width
     * @param outHeight Output image height
     * @return Image data (BGRA format, 32-bit)，Returns empty vector on failure
     */
    virtual std::vector<uint8_t> getImage(int* outWidth, int* outHeight) = 0;

    /**
     * @brief Check if clipboard contains image
     * @return Returns true if image is present
     */
    virtual bool hasImage() = 0;

    // ========== File operations ==========

    /**
     * @brief Set clipboard file list
     * @param files File path list
     * @return Returns true on success
     */
    virtual bool setFiles(const std::vector<std::string>& files) = 0;

    /**
     * @brief Get clipboard file list
     * @return File path list，Returns empty vector on failure
     */
    virtual std::vector<std::string> getFiles() = 0;

    /**
     * @brief Check if clipboard contains files
     * @return Returns true if files are present
     */
    virtual bool hasFiles() = 0;

    // ========== General operations ==========

    /**
     * @brief Clear clipboard
     */
    virtual void clear() = 0;

    /**
     * @brief Check if clipboard is empty
     * @return Returns true if empty
     */
    virtual bool isEmpty() = 0;

    /**
     * @brief Get available format list
     * @return Format list
     */
    virtual std::vector<ClipboardFormat> getAvailableFormats() = 0;

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
