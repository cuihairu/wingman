#ifdef __linux__

#include "wingman/platform/iclipboard.hpp"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

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

        // Use pipe to xclip without shell to avoid injection
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            spdlog::error("X11Clipboard: pipe() failed");
            return false;
        }

        pid_t pid = fork();
        if (pid == -1) {
            spdlog::error("X11Clipboard: fork() failed");
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }

        if (pid == 0) {
            // Child process
            close(pipefd[1]);  // Close write end
            dup2(pipefd[0], STDIN_FILENO);  // Redirect pipe to stdin
            close(pipefd[0]);

            // Execute xclip directly (no shell)
            execlp("xclip", "xclip", "-selection", "clipboard", nullptr);

            // If execlp returns, execution failed
            _exit(1);
        } else {
            // Parent process
            close(pipefd[0]);  // Close read end

            // Write data to pipe
            ssize_t written = write(pipefd[1], text.c_str(), text.size());
            close(pipefd[1]);

            // Wait for child and check status
            int status;
            waitpid(pid, &status, 0);

            return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                   written == static_cast<ssize_t>(text.size());
        }
    }

    std::string getText() override {
        if (!initialized_) return "";

        // Use pipe to xclip without shell
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            spdlog::error("X11Clipboard: pipe() failed for read");
            return "";
        }

        pid_t pid = fork();
        if (pid == -1) {
            spdlog::error("X11Clipboard: fork() failed for read");
            close(pipefd[0]);
            close(pipefd[1]);
            return "";
        }

        if (pid == 0) {
            // Child process
            close(pipefd[0]);  // Close read end
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
            close(pipefd[1]);

            // Redirect stderr to /dev/null
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull != -1) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            // Execute xclip directly (no shell)
            execlp("xclip", "xclip", "-selection", "clipboard", "-o", nullptr);

            // If execlp returns, execution failed
            _exit(1);
        } else {
            // Parent process
            close(pipefd[1]);  // Close write end

            std::string result;
            char buffer[4096];
            ssize_t bytesRead;
            while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
                result.append(buffer, bytesRead);
            }
            close(pipefd[0]);

            // Wait for child
            int status;
            waitpid(pid, &status, 0);

            return result;
        }
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
