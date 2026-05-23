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

using PlatformClipboard = wingman::platform::NullClipboard;
#endif

namespace wingman {

// ========== Clipboard 实现 ==========

platform::IClipboard& Clipboard::instance() {
    static std::unique_ptr<PlatformClipboard> instance = [] {
        auto clipboard = std::make_unique<PlatformClipboard>();
        if (!clipboard->initialize()) {
            spdlog::error("[Clipboard] Failed to initialize platform clipboard");
        }
        return clipboard;
    }();
    return *instance;
}

// ========== 便捷静态方法 ==========

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
