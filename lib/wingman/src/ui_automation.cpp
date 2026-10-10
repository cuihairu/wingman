#include "wingman/ui_automation.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>

// 平台工厂符号（win_automation.cpp / mac_automation.cpp 提供；Linux 等无
// UIA 后端平台由 linux_ui_automation_factory.cpp 返回 nullptr stub）。声明
// 收敛在薄层头文件，调用点保持平台中立（薄层纪律，
// docs/platform-abstraction-design.md §8）。
#include "platform/ui_automation_factory.hpp"

namespace wingman {

// 文件内别名：IUIAManager::UIAEventCallback 在本 .cpp 中简写为 UIAEventCallback，
// 与 UIAutomation 门面成员类型保持一致。
using UIAEventCallback = IUIAManager::UIAEventCallback;

// ========== UIAutomation::Impl ==========

struct UIAutomation::Impl {
    std::unique_ptr<IUIAManager> manager;

    bool initialize() {
        if (manager) return true;

        manager = ::createUIAManager();

        if (!manager) {
            spdlog::warn("[UIAutomation] No UI Automation support for this platform");
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
        if (!manager) return nullptr;
        return manager->findElement(selector);
    }

    std::shared_ptr<IUIAElement> getFocused() {
        if (!manager) return nullptr;
        return manager->getFocusedElement();
    }

    std::shared_ptr<IUIAElement> fromPoint(int x, int y) {
        if (!manager) return nullptr;
        return manager->getElementFromPoint(Point{x, y});
    }

    // Plan 6: 新增委托方法
    std::shared_ptr<IUIAElement> fromWindow(uint64_t hwnd) {
        if (!manager) return nullptr;
        return manager->getElementFromWindow(hwnd);
    }

    std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role) {
        if (!manager) return {};
        return manager->findAllByRole(role);
    }

    std::shared_ptr<IUIAElement> waitFor(const UIASelector& selector, int timeoutMs) {
        if (!manager) return nullptr;
        return manager->waitForElement(selector, timeoutMs);
    }

    // Plan 6 Phase 2: 事件监听委托
    uint64_t addPropertyChangedListener(const UIASelector& selector, UIAEventCallback cb) {
        if (!manager) return 0;
        return manager->addPropertyChangedListener(selector, std::move(cb));
    }
    uint64_t addStructureChangedListener(const UIASelector& selector, UIAEventCallback cb) {
        if (!manager) return 0;
        return manager->addStructureChangedListener(selector, std::move(cb));
    }
    bool removeEventListener(uint64_t listenerId) {
        if (!manager) return false;
        return manager->removeEventListener(listenerId);
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

// Plan 6: 新增门面方法实现
std::shared_ptr<IUIAElement> UIAutomation::fromWindow(uint64_t hwnd) {
    return impl->fromWindow(hwnd);
}

std::vector<std::shared_ptr<IUIAElement>> UIAutomation::findAllByRole(UIARole role) {
    return impl->findAllByRole(role);
}

std::shared_ptr<IUIAElement> UIAutomation::waitForName(const std::string& name, int timeoutMs) {
    return impl->waitFor(UIASelector{}.withName(name), timeoutMs);
}

std::shared_ptr<IUIAElement> UIAutomation::waitFor(const UIASelector& selector, int timeoutMs) {
    return impl->waitFor(selector, timeoutMs);
}

std::shared_ptr<IUIAElement> UIAutomation::waitForId(const std::string& id, int timeoutMs) {
    return impl->waitFor(UIASelector{}.withId(id), timeoutMs);
}

std::shared_ptr<IUIAElement> UIAutomation::waitForRole(UIARole role, int timeoutMs) {
    return impl->waitFor(UIASelector{}.withRole(role), timeoutMs);
}

// Plan 6 Phase 2: 事件监听门面实现
uint64_t UIAutomation::addPropertyChangedListener(const UIASelector& selector, UIAEventCallback cb) {
    return impl->addPropertyChangedListener(selector, std::move(cb));
}
uint64_t UIAutomation::addStructureChangedListener(const UIASelector& selector, UIAEventCallback cb) {
    return impl->addStructureChangedListener(selector, std::move(cb));
}
bool UIAutomation::removeEventListener(uint64_t listenerId) {
    return impl->removeEventListener(listenerId);
}

// ========== Global Access ==========

namespace {
    std::unique_ptr<UIAutomation> g_instance;
    std::once_flag g_initFlag;
}

UIAutomation& uia() {
    std::call_once(g_initFlag, []() {
        g_instance = std::make_unique<UIAutomation>();
        // 生产链路懒初始化：首个脚本调用即尝试建立平台 manager。
        // 平台不支持或初始化失败仅记录日志，后续调用按未初始化降级（查找返空）。
        g_instance->initialize();
    });
    return *g_instance;
}

} // namespace wingman
