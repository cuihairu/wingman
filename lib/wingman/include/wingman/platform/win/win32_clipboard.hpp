#pragma once

#include "wingman/platform/iclipboard.hpp"
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace wingman::platform::windows {

/**
 * @brief Windows 剪贴板实现
 *
 * 支持 CF_TEXT, CF_UNICODETEXT, CF_HDROP, CF_DIB 等格式。
 */
class Win32Clipboard : public IClipboard {
public:
    Win32Clipboard();
    ~Win32Clipboard() override;

    bool initialize() override;
    void shutdown() override;

    // ========== 文本操作 ==========

    bool setText(const std::string& text) override;
    std::string getText() override;
    bool hasText() override;

    // ========== HTML 操作 ==========

    bool setHTML(const std::string& html) override;
    std::string getHTML() override;
    bool hasHTML() override;

    // ========== 图像操作 ==========

    bool setImage(const std::vector<uint8_t>& imageData, int width, int height) override;
    std::vector<uint8_t> getImage(int* outWidth, int* outHeight) override;
    bool hasImage() override;

    // ========== 文件操作 ==========

    bool setFiles(const std::vector<std::string>& files) override;
    std::vector<std::string> getFiles() override;
    bool hasFiles() override;

    // ========== 通用操作 ==========

    void clear() override;
    bool isEmpty() override;
    std::vector<ClipboardFormat> getAvailableFormats() override;

    std::string getBackendName() const override { return "Win32"; }
    BackendInfo getBackendInfo() const override;

private:
    bool initialized_;
    bool opened_;

    bool openClipboard();
    void closeClipboard();
    static UINT getHtmlFormat();
};

} // namespace wingman::platform::windows

#endif // _WIN32
