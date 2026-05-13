#include "wingman/ui_automation.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>

// 平台特定的管理器创建函数
#ifdef _WIN32
extern std::unique_ptr<wingman::IUIAManager> createUIAManager();
#elif defined(__APPLE__)
extern std::unique_ptr<wingman::IUIAManager> createUIAManager();
#endif

namespace wingman {

// ========== UIAutomation::Impl ==========

struct UIAutomation::Impl {
    std::unique_ptr<IUIAManager> manager;

    bool initialize() {
        if (manager) return true;

#ifdef _WIN32
        manager = ::createUIAManager();
#elif defined(__APPLE__)
        manager = ::createUIAManager();
#else
        spdlog::warn("[UIAutomation] No UI Automation support for this platform");
        return false;
#endif

        if (!manager) {
            spdlog::error("[UIAutomation] Failed to create manager");
            return false;
        }

        if (!manager->initialize()) {
            spdlog::error("[UIAutomation] Failed to initialize manager: {}",
                        manager->getBackendName());
            return false;
        }

        spdlog::info("[UIAutomation] Initialized with backend: {}", manager->getBackendName());
        return true;
    }

    void cleanup() {
        if (manager) {
            manager->shutdown();
            manager.reset();
        }
    }

    std::shared_ptr<IUIAElement> find(const UIASelector& selector) {
        ensureInitialized();
        if (!manager) return nullptr;
        return manager->findElement(selector);
    }

    std::shared_ptr<IUIAElement> getFocused() {
        ensureInitialized();
        if (!manager) return nullptr;
        return manager->getFocusedElement();
    }

    std::shared_ptr<IUIAElement> fromPoint(int x, int y) {
        ensureInitialized();
        if (!manager) return nullptr;
        return manager->getElementFromPoint(Point{x, y});
    }

private:
    void ensureInitialized() {
        if (!manager) {
            initialize();
        }
    }
};

// ========== UIAutomation ==========

UIAutomation::UIAutomation() : impl(new Impl()) {}

UIAutomation::~UIAutomation() {
    impl->cleanup();
}

bool UIAutomation::initialize() {
    return impl->initialize();
}

void UIAutomation::cleanup() {
    impl->cleanup();
}

std::shared_ptr<IUIAElement> UIAutomation::find(const UIASelector& selector) {
    return impl->find(selector);
}

std::shared_ptr<IUIAElement> UIAutomation::findByName(const std::string& name) {
    return find(UIASelector{}.withName(name));
}

std::shared_ptr<IUIAElement> UIAutomation::findById(const std::string& id) {
    return find(UIASelector{}.withId(id));
}

std::shared_ptr<IUIAElement> UIAutomation::getFocusedElement() {
    return impl->getFocused();
}

std::shared_ptr<IUIAElement> UIAutomation::getElementFromPoint(int x, int y) {
    return impl->fromPoint(x, y);
}

// ========== 全局访问 ==========

namespace {
    std::unique_ptr<UIAutomation> g_instance;
    std::once_flag g_initFlag;
}

UIAutomation& uia() {
    std::call_once(g_initFlag, []() {
        g_instance = std::make_unique<UIAutomation>();
    });
    return *g_instance;
}

} // namespace wingman
