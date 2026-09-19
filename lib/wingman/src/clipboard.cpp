#include "wingman/clipboard.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "platform/win/win32_clipboard.hpp"
using PlatformClipboard = wingman::platform::win::Win32Clipboard;
#else

namespace wingman::platform {

class NullClipboard final : public IClipboard {
public:
    bool initialize() override { return true; }
    void shutdown() override {}

    bool setText(const std::string&) override { return false; }
    std::string getText() override { return {}; }
    bool hasText() override { return false; }

    bool setHTML(const std::string&) override { return false; }
    std::string getHTML() override { return {}; }
    bool hasHTML() override { return false; }

    bool setImage(const std::vector<uint8_t>&, int, int) override { return false; }
    std::vector<uint8_t> getImage(int* outWidth, int* outHeight) override {
        if (outWidth) {
            *outWidth = 0;
        }
        if (outHeight) {
            *outHeight = 0;
        }
        return {};
    }
    bool hasImage() override { return false; }

    bool setFiles(const std::vector<std::string>&) override { return false; }
    std::vector<std::string> getFiles() override { return {}; }
    bool hasFiles() override { return false; }

    void clear() override {}
    bool isEmpty() override { return true; }
    std::vector<ClipboardFormat> getAvailableFormats() override { return {}; }

    std::string getBackendName() const override { return "Null"; }
    BackendInfo getBackendInfo() const override {
        return BackendInfo{"Null", "1.0", true, "No-op clipboard backend"};
    }
};

} // namespace wingman::platform

// Linux/macOS：接入平台后端（工厂在各自平台源文件导出）。此前两平台此分支
// 恒为 NullClipboard（装配断链：Linux 2026-09-14、macOS 2026-09-16 接线）。
#if defined(__linux__)
// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间前需要 undo（同 x11_factory.cpp）
#undef linux
namespace wingman::platform::linux {
std::unique_ptr<IClipboard> createX11Clipboard();
}
#define WINGMAN_HAS_CLIPBOARD_FACTORY 1
#elif defined(__APPLE__)
namespace wingman::platform::mac {
std::unique_ptr<IClipboard> createCocoaClipboard();
}
#define WINGMAN_HAS_CLIPBOARD_FACTORY 1
#endif

using PlatformClipboard = wingman::platform::NullClipboard;
#endif

namespace wingman {

// ========== Clipboard Implementation ==========

platform::IClipboard& Clipboard::instance() {
    static std::unique_ptr<platform::IClipboard> instance = [] {
#if defined(WINGMAN_HAS_CLIPBOARD_FACTORY)
        // 平台工厂内部已 initialize()，装配处不得二次调用——X11 侧会覆盖
        // display_ 指针并泄漏首次 X 连接（Cocoa 侧 initialize 幂等，保持同一
        // 模式）。无 DISPLAY/无 xclip 时 X11Clipboard 自身优雅降级（setText
        // 返回 false），与 Null 语义一致。
#if defined(__linux__)
        auto clipboard = platform::linux::createX11Clipboard();
#else
        auto clipboard = platform::mac::createCocoaClipboard();
#endif
        if (!clipboard) {
            clipboard = std::make_unique<platform::NullClipboard>();
        }
#else
        auto clipboard = std::make_unique<PlatformClipboard>();
        if (!clipboard->initialize()) {
            spdlog::error("[Clipboard] Failed to initialize platform clipboard");
        }
#endif
        return clipboard;
    }();
    return *instance;
}

// ========== Convenience Static Methods ==========

bool Clipboard::setText(const std::string& text) {
    return instance().setText(text);
}

std::string Clipboard::getText() {
    return instance().getText();
}

bool Clipboard::hasText() {
    return instance().hasText();
}

bool Clipboard::setHTML(const std::string& html) {
    return instance().setHTML(html);
}

std::string Clipboard::getHTML() {
    return instance().getHTML();
}

bool Clipboard::hasHTML() {
    return instance().hasHTML();
}

bool Clipboard::setImage(const std::vector<uint8_t>& imageData, int width, int height) {
    return instance().setImage(imageData, width, height);
}

std::vector<uint8_t> Clipboard::getImage(int* outWidth, int* outHeight) {
    return instance().getImage(outWidth, outHeight);
}

bool Clipboard::hasImage() {
    return instance().hasImage();
}

bool Clipboard::setFiles(const std::vector<std::string>& files) {
    return instance().setFiles(files);
}

std::vector<std::string> Clipboard::getFiles() {
    return instance().getFiles();
}

bool Clipboard::hasFiles() {
    return instance().hasFiles();
}

void Clipboard::clear() {
    instance().clear();
}

bool Clipboard::isEmpty() {
    return instance().isEmpty();
}

} // namespace wingman
