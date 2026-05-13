#include "wingman/platform/platform_factory.hpp"
#include "wingman/platform/icapture.hpp"
#include "wingman/platform/iinput.hpp"
#include "wingman/platform/iwindow.hpp"
#include "wingman/platform/iscreen.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

// 平台特定实现
#ifdef _WIN32
namespace win_capture = wingman::platform::win;
namespace win_input = wingman::platform::win;
namespace win_window = wingman::platform::win;
namespace win_screen = wingman::platform::win;

extern "C" {
    std::unique_ptr<ICapture> CreateGDICapture();
    std::unique_ptr<IInput> CreateSendInputInput();
    std::unique_ptr<IWindow> CreateWin32Window();
    std::unique_ptr<IScreen> CreateWin32Screen();
}
#endif

#ifdef __APPLE__
namespace mac_capture = wingman::platform::mac;
namespace mac_input = wingman::platform::mac;
namespace mac_window = wingman::platform::mac;
namespace mac_screen = wingman::platform::mac;

extern "C" {
    std::unique_ptr<ICapture> CreateSCCapture();
    std::unique_ptr<IInput> CreateCGEventInput();
    std::unique_ptr<IWindow> CreateCocoaWindow();
    std::unique_ptr<IScreen> CreateCocoaScreen();
}
#endif

namespace wingman::platform {

// Windows 平台工厂
#ifdef _WIN32

class WindowsPlatformFactory : public IPlatformFactory {
public:
    std::unique_ptr<ICapture> createCapture(const CaptureConfig& config) override {
        auto backend = config.preferredBackend;
        if (backend == CaptureBackend::Auto) {
            backend = CaptureBackend::GDI;  // 默认使用 GDI
        }

        switch (backend) {
            case CaptureBackend::GDI:
            case CaptureBackend::Auto:
                // 返回 GDICapture 实例
                return std::unique_ptr<ICapture>(new win_capture::GDICapture());

            default:
                spdlog::warn("[WindowsPlatformFactory] Unsupported capture backend: {}",
                            getCaptureBackendName(backend));
                return nullptr;
        }
    }

    std::unique_ptr<IInput> createInput(const InputConfig& config) override {
        auto backend = config.preferredBackend;
        if (backend == InputBackend::Auto) {
            backend = InputBackend::SendInput;
        }

        switch (backend) {
            case InputBackend::SendInput:
            case InputBackend::Auto:
                return std::unique_ptr<IInput>(new win_input::SendInputInput());

            default:
                spdlog::warn("[WindowsPlatformFactory] Unsupported input backend: {}",
                            getInputBackendName(backend));
                return nullptr;
        }
    }

    std::unique_ptr<IWindow> createWindow() override {
        return std::unique_ptr<IWindow>(new win_window::Win32Window());
    }

    std::unique_ptr<IScreen> createScreen() override {
        return std::unique_ptr<IScreen>(new win_screen::Win32Screen());
    }

    std::string getPlatformName() const override {
        return "Windows";
    }

    bool supportsCaptureBackend(CaptureBackend backend) const override {
        return backend == CaptureBackend::Auto ||
               backend == CaptureBackend::GDI ||
               backend == CaptureBackend::Mock;
    }

    bool supportsInputBackend(InputBackend backend) const override {
        return backend == InputBackend::Auto ||
               backend == InputBackend::SendInput ||
               backend == InputBackend::Mock;
    }

    CaptureBackend getRecommendedCaptureBackend() const override {
        return CaptureBackend::GDI;
    }

    InputBackend getRecommendedInputBackend() const override {
        return InputBackend::SendInput;
    }

    std::vector<CaptureBackend> getAvailableCaptureBackends() const override {
        return {CaptureBackend::GDI};
    }

    std::vector<InputBackend> getAvailableInputBackends() const override {
        return {InputBackend::SendInput};
    }
};

#endif // _WIN32

// macOS 平台工厂
#ifdef __APPLE__

class MacOSPlatformFactory : public IPlatformFactory {
public:
    std::unique_ptr<ICapture> createCapture(const CaptureConfig& config) override {
        auto backend = config.preferredBackend;
        if (backend == CaptureBackend::Auto) {
            // 优先使用 ScreenCaptureKit (macOS 12.3+)
            backend = CaptureBackend::ScreenCaptureKit;
        }

        switch (backend) {
            case CaptureBackend::ScreenCaptureKit:
            case CaptureBackend::Auto:
                return std::unique_ptr<ICapture>(new mac_capture::SCCapture());

            case CaptureBackend::CoreGraphics:
                // TODO: 实现 CGCapture
                spdlog::warn("[MacOSPlatformFactory] CoreGraphics capture not implemented yet");
                return nullptr;

            default:
                spdlog::warn("[MacOSPlatformFactory] Unsupported capture backend: {}",
                            getCaptureBackendName(backend));
                return nullptr;
        }
    }

    std::unique_ptr<IInput> createInput(const InputConfig& config) override {
        auto backend = config.preferredBackend;
        if (backend == InputBackend::Auto) {
            backend = InputBackend::CGEvent;
        }

        switch (backend) {
            case InputBackend::CGEvent:
            case InputBackend::Auto:
                return std::unique_ptr<IInput>(new mac_input::CGEventInput());

            default:
                spdlog::warn("[MacOSPlatformFactory] Unsupported input backend: {}",
                            getInputBackendName(backend));
                return nullptr;
        }
    }

    std::unique_ptr<IWindow> createWindow() override {
        return std::unique_ptr<IWindow>(new mac_window::CocoaWindow());
    }

    std::unique_ptr<IScreen> createScreen() override {
        return std::unique_ptr<IScreen>(new mac_screen::CocoaScreen());
    }

    std::string getPlatformName() const override {
        return "macOS";
    }

    bool supportsCaptureBackend(CaptureBackend backend) const override {
        return backend == CaptureBackend::Auto ||
               backend == CaptureBackend::ScreenCaptureKit ||
               backend == CaptureBackend::CoreGraphics ||
               backend == CaptureBackend::Mock;
    }

    bool supportsInputBackend(InputBackend backend) const override {
        return backend == InputBackend::Auto ||
               backend == InputBackend::CGEvent ||
               backend == InputBackend::Mock;
    }

    CaptureBackend getRecommendedCaptureBackend() const override {
        return CaptureBackend::ScreenCaptureKit;
    }

    InputBackend getRecommendedInputBackend() const override {
        return InputBackend::CGEvent;
    }

    std::vector<CaptureBackend> getAvailableCaptureBackends() const override {
        return {CaptureBackend::ScreenCaptureKit};
    }

    std::vector<InputBackend> getAvailableInputBackends() const override {
        return {InputBackend::CGEvent};
    }
};

#endif // __APPLE__

// 全局工厂实例
namespace {
    std::unique_ptr<IPlatformFactory> g_customFactory;
}

// 创建默认平台工厂
static std::unique_ptr<IPlatformFactory> createDefaultPlatformFactory() {
#ifdef _WIN32
    return std::unique_ptr<IPlatformFactory>(new WindowsPlatformFactory());
#elif defined(__APPLE__)
    return std::unique_ptr<IPlatformFactory>(new MacOSPlatformFactory());
#else
    #error "Unsupported platform"
#endif
}

// 全局访问
IPlatformFactory& getPlatformFactory() {
    if (g_customFactory) {
        return *g_customFactory;
    }

    static std::unique_ptr<IPlatformFactory> defaultFactory = createDefaultPlatformFactory();
    return *defaultFactory;
}

void setPlatformFactory(std::unique_ptr<IPlatformFactory> factory) {
    g_customFactory = std::move(factory);
}

} // namespace wingman::platform
