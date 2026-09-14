#include "wingman/clipboard.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "wingman/platform/win/win32_clipboard.hpp"
using PlatformClipboard = wingman::platform::windows::Win32Clipboard;
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

// Linux：接入 X11/xclip 后端（x11_factory.cpp 聚合导出）。此前此分支恒为
// NullClipboard，X11Clipboard 全库零消费者（装配断链）。
// macOS 暂无剪贴板工厂导出，维持 Null 兜底（见 todo.md）。
#if defined(__linux__)
// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间前需要 undo（同 x11_factory.cpp）
#undef linux
namespace wingman::platform::linux {
std::unique_ptr<IClipboard> createX11Clipboard();
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
        // X11 工厂内部已 initialize()（XOpenDisplay + atoms），装配处不得二次
        // 调用——会覆盖 display_ 指针并泄漏首次 X 连接。无 DISPLAY/无 xclip 时
        // X11Clipboard 自身优雅降级（setText 返回 false），与 Null 语义一致。
        auto clipboard = platform::linux::createX11Clipboard();
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
