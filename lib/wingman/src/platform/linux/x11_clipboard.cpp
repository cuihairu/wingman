#ifdef __linux__

#include "wingman/platform/iclipboard.hpp"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <cstring>
#include <string>

namespace wingman::platform::linux {

class X11Clipboard : public IClipboard {
public:
    X11Clipboard() = default;
    ~X11Clipboard() override { shutdown(); }

    bool initialize() override {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("X11Clipboard: failed to open X display");
            return false;
        }
        root_ = DefaultRootWindow(display_);
        clipboardAtom_ = XInternAtom(display_, "CLIPBOARD", False);
        utf8Atom_ = XInternAtom(display_, "UTF8_STRING", False);
        xaString_ = XInternAtom(display_, "STRING", False);
        targetsAtom_ = XInternAtom(display_, "TARGETS", False);
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (display_) { XCloseDisplay(display_); display_ = nullptr; }
        initialized_ = false;
    }

    bool setText(const std::string& text) override {
        if (!initialized_) return false;
        // Use xclip as a reliable cross-toolkit method
        std::string cmd = "echo -n '" + escapeShell(text) + "' | xclip -selection clipboard";
        return system(cmd.c_str()) == 0;
    }

    std::string getText() override {
        if (!initialized_) return "";
        // Use xclip to read clipboard
        FILE* pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
        if (!pipe) return "";
        std::string result;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    }

    bool hasText() override {
        return !getText().empty();
    }

    bool setHTML(const std::string& html) override {
        // Store HTML as text with markup
        return setText(html);
    }

    std::string getHTML() override {
        return getText();
    }

    bool hasHTML() override {
        return hasText();
    }

    bool setImage(const std::vector<uint8_t>& imageData, int width, int height) override {
        // X11 clipboard image support is complex; not implemented in MVP
        return false;
    }

    std::vector<uint8_t> getImage(int* outWidth, int* outHeight) override {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        return {};
    }

    bool hasImage() override { return false; }

    bool setFiles(const std::vector<std::string>& files) override {
        if (files.empty()) return false;
        std::string joined;
        for (size_t i = 0; i < files.size(); i++) {
            if (i > 0) joined += "\n";
            joined += files[i];
        }
        return setText(joined);
    }

    std::vector<std::string> getFiles() override {
        std::string text = getText();
        std::vector<std::string> files;
        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find('\n', start);
            if (end == std::string::npos) end = text.size();
            std::string line = text.substr(start, end - start);
            if (!line.empty()) files.push_back(line);
            start = end + 1;
        }
        return files;
    }

    bool hasFiles() override {
        auto files = getFiles();
        return !files.empty();
    }

    void clear() override {
        setText("");
    }

    bool isEmpty() override {
        return getText().empty();
    }

    std::vector<ClipboardFormat> getAvailableFormats() override {
        std::vector<ClipboardFormat> formats;
        if (hasText()) formats.push_back(ClipboardFormat::Text);
        return formats;
    }

    std::string getBackendName() const override { return "X11/xclip"; }
    BackendInfo getBackendInfo() const override {
        return {"X11/xclip", "1.0", initialized_, "X11 clipboard via xclip"};
    }

private:
    Display* display_ = nullptr;
    Window root_ = 0;
    bool initialized_ = false;
    Atom clipboardAtom_, utf8Atom_, xaString_, targetsAtom_;

    static std::string escapeShell(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        return out;
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
