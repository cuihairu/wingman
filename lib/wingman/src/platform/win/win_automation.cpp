#ifdef _WIN32
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <atlbase.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uiautomationcore.lib")
#endif

#include "wingman/ui_automation.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

#ifdef _WIN32

namespace wingman {

class UIAElement : public IUIAElement {
public:
    explicit UIAElement(CComPtr<IUIAutomationElement> element, CComPtr<IUIAutomation> automation = nullptr)
        : element_(element), automation_(automation) {}

    std::string getName() const override {
        if (!element_) return "";
        CComBSTR name;
        if (SUCCEEDED(element_->get_CurrentName(&name)) && name) {
            CW2A converted(name);
            return std::string(converted);
        }
        return "";
    }

    std::string getId() const override {
        if (!element_) return "";
        CComBSTR bstr;
        if (SUCCEEDED(element_->get_CurrentAutomationId(&bstr)) && bstr) {
            CW2A converted(bstr);
            return std::string(converted);
        }
        return "";
    }

    std::string getClassName() const override {
        if (!element_) return "";
        CComBSTR bstr;
        if (SUCCEEDED(element_->get_CurrentClassName(&bstr)) && bstr) {
            CW2A converted(bstr);
            return std::string(converted);
        }
        return "";
    }

    UIARole getRole() const override {
        CONTROLTYPEID controlType = 0;
        if (element_ && SUCCEEDED(element_->get_CurrentControlType(&controlType))) {
            return controlTypeToRole(controlType);
        }
        return UIARole::Unknown;
    }

    Rect getBounds() const override {
        if (!element_) return Rect{};
        RECT rect;
        if (SUCCEEDED(element_->get_CurrentBoundingRectangle(&rect))) {
            return Rect{rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
        }
        return Rect{};
    }

    bool isVisible() const override {
        if (!element_) return false;
        BOOL isOffscreen = FALSE;
        return SUCCEEDED(element_->get_CurrentIsOffscreen(&isOffscreen)) && !isOffscreen;
    }

    bool isEnabled() const override {
        if (!element_) return false;
        BOOL isEnabled = FALSE;
        return SUCCEEDED(element_->get_CurrentIsEnabled(&isEnabled)) && isEnabled;
    }

    std::string getText() const override {
        CComPtr<IUIAutomationValuePattern> valuePattern;
        if (element_ && SUCCEEDED(element_->GetCurrentPatternAs(UIA_ValuePatternId,
            __uuidof(IUIAutomationValuePattern), reinterpret_cast<void**>(&valuePattern))) && valuePattern) {
            CComBSTR value;
            if (SUCCEEDED(valuePattern->get_CurrentValue(&value)) && value) {
                CW2A converted(value);
                return std::string(converted);
            }
        }
        return getName();
    }

    bool setText(const std::string& text) override {
        CComPtr<IUIAutomationValuePattern> valuePattern;
        if (element_ && SUCCEEDED(element_->GetCurrentPatternAs(UIA_ValuePatternId,
            __uuidof(IUIAutomationValuePattern), reinterpret_cast<void**>(&valuePattern))) && valuePattern) {
            CComBSTR value = CComBSTR(text.c_str());
            return SUCCEEDED(valuePattern->SetValue(value));
        }
        return false;
    }

    bool click() override {
        CComPtr<IUIAutomationInvokePattern> invokePattern;
        if (element_ && SUCCEEDED(element_->GetCurrentPatternAs(UIA_InvokePatternId,
            __uuidof(IUIAutomationInvokePattern), reinterpret_cast<void**>(&invokePattern))) && invokePattern) {
            return SUCCEEDED(invokePattern->Invoke());
        }
        return false;
    }

    bool setFocus() override {
        return element_ && SUCCEEDED(element_->SetFocus());
    }

    bool hasFocus() const override {
        if (!element_) return false;
        BOOL hasFocus = FALSE;
        return SUCCEEDED(element_->get_CurrentHasKeyboardFocus(&hasFocus)) && hasFocus;
    }

    bool isChecked() const override {
        CComPtr<IUIAutomationTogglePattern> togglePattern;
        if (element_ && SUCCEEDED(element_->GetCurrentPatternAs(UIA_TogglePatternId,
            __uuidof(IUIAutomationTogglePattern), reinterpret_cast<void**>(&togglePattern))) && togglePattern) {
            ToggleState state;
            if (SUCCEEDED(togglePattern->get_CurrentToggleState(&state))) {
                return state == ToggleState_On;
            }
        }
        return false;
    }

    bool setChecked(bool checked) override {
        CComPtr<IUIAutomationTogglePattern> togglePattern;
        if (element_ && SUCCEEDED(element_->GetCurrentPatternAs(UIA_TogglePatternId,
            __uuidof(IUIAutomationTogglePattern), reinterpret_cast<void**>(&togglePattern))) && togglePattern) {
            // Check current state first
            ToggleState current;
            if (SUCCEEDED(togglePattern->get_CurrentToggleState(&current))) {
                bool isCurrentlyChecked = (current == ToggleState_On);
                if (isCurrentlyChecked != checked) {
                    // State mismatch, toggle it
                    return SUCCEEDED(togglePattern->Toggle());
                }
                return true;
            }
        }
        return false;
    }

    bool isValid() const override {
        if (!element_) return false;
        BOOL isOffscreen = FALSE;
        return SUCCEEDED(element_->get_CurrentIsOffscreen(&isOffscreen));
    }

    UIAElementInfo getInfo() const override {
        UIAElementInfo info;
        info.name = getName();
        info.id = getId();
        info.className = getClassName();
        info.role = getRole();
        info.bounds = getBounds();
        info.isEnabled = isEnabled();
        info.isVisible = isVisible();
        info.hasFocus = hasFocus();
        info.text = getText();
        return info;
    }

    // 平台内部访问器：UIAManager 事件监听用（同文件 static_cast 访问）
    CComPtr<IUIAutomationElement> elementRef() const { return element_; }

    // Plan 6: 新增方法实现
    std::vector<std::shared_ptr<IUIAElement>> getChildren() override {
        std::vector<std::shared_ptr<IUIAElement>> results;
        if (!element_ || !automation_) return results;

        CComPtr<IUIAutomationTreeWalker> walker;
        if (FAILED(automation_->get_RawViewWalker(&walker)) || !walker) return results;

        CComPtr<IUIAutomationElement> child;
        if (FAILED(walker->GetFirstChildElement(element_, &child))) return results;

        while (child) {
            results.push_back(std::make_shared<UIAElement>(child, automation_));
            CComPtr<IUIAutomationElement> next;
            if (FAILED(walker->GetNextSiblingElement(child, &next))) break;
            child = next;
        }
        return results;
    }

    bool expand() override {
        if (!element_) return false;
        CComPtr<IUIAutomationExpandCollapsePattern> pattern;
        if (FAILED(element_->GetCurrentPatternAs(__uuidof(IUIAutomationExpandCollapsePattern),
            reinterpret_cast<void**>(&pattern))) || !pattern) return false;
        return SUCCEEDED(pattern->Expand());
    }

    bool collapse() override {
        if (!element_) return false;
        CComPtr<IUIAutomationExpandCollapsePattern> pattern;
        if (FAILED(element_->GetCurrentPatternAs(__uuidof(IUIAutomationExpandCollapsePattern),
            reinterpret_cast<void**>(&pattern))) || !pattern) return false;
        return SUCCEEDED(pattern->Collapse());
    }

    bool isExpanded() const override {
        if (!element_) return false;
        CComPtr<IUIAutomationExpandCollapsePattern> pattern;
        if (FAILED(element_->GetCurrentPatternAs(__uuidof(IUIAutomationExpandCollapsePattern),
            reinterpret_cast<void**>(&pattern))) || !pattern) return false;
        ExpandCollapseState state;
        if (FAILED(pattern->get_CurrentExpandCollapseState(&state))) return false;
        return state == ExpandCollapseState_Expanded;
    }

    bool doubleClick() override {
        // 简单实现：调用两次 click
        if (!click()) return false;
        return click();
    }

private:
    CComPtr<IUIAutomationElement> element_;
    CComPtr<IUIAutomation> automation_;  // Plan 6: 持有 automation 引用，用于 TreeWalker 等

    static UIARole controlTypeToRole(CONTROLTYPEID controlType) {
        switch (controlType) {
            case UIA_ButtonControlTypeId: return UIARole::Button;
            case UIA_CheckBoxControlTypeId: return UIARole::CheckBox;
            case UIA_ComboBoxControlTypeId: return UIARole::ComboBox;
            case UIA_EditControlTypeId: return UIARole::TextBox;
            case UIA_WindowControlTypeId: return UIARole::Window;
            case UIA_RadioButtonControlTypeId: return UIARole::RadioButton;
            default: return UIARole::Unknown;
        }
    }
};

// ========== Plan 6 Phase 2: Windows UIA 事件监听（COM）==========
// COM event handler 回调从 UIA 内部 RPC 线程触发，在其中调用脚本 callable。
// callable 的线程安全由脚本层（on_property_changed 等）通过 callableThreadSafe 检查保证。
class WinUiaPropertyChangedHandler : public IUIAutomationPropertyChangedEventHandler {
public:
    WinUiaPropertyChangedHandler(CComPtr<IUIAutomation> automation, wingman::IUIAManager::UIAEventCallback cb)
        : automation_(automation), callback_(std::move(cb)), refCount_(1) {}

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return static_cast<ULONG>(c);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IUIAutomationPropertyChangedEventHandler)) {
            *ppv = static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID /*propertyId*/) override {
        if (callback_ && sender) {
            try {
                callback_(std::make_shared<UIAElement>(sender, automation_));
            } catch (const std::exception& e) {
                spdlog::error("[UIA] Win 属性变化回调异常: {}", e.what());
            } catch (...) {
                spdlog::error("[UIA] Win 属性变化回调未知异常");
            }
        }
        return S_OK;
    }
private:
    CComPtr<IUIAutomation> automation_;
    wingman::IUIAManager::UIAEventCallback callback_;
    LONG refCount_;
};

class WinUiaStructureChangedHandler : public IUIAutomationStructureChangedEventHandler {
public:
    WinUiaStructureChangedHandler(CComPtr<IUIAutomation> automation, wingman::IUIAManager::UIAEventCallback cb)
        : automation_(automation), callback_(std::move(cb)), refCount_(1) {}

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return static_cast<ULONG>(c);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IUIAutomationStructureChangedEventHandler)) {
            *ppv = static_cast<IUIAutomationStructureChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(IUIAutomationElement* sender, StructureChangeType /*changeType*/, SAFEARRAY* /*runtimeId*/) override {
        if (callback_ && sender) {
            try {
                callback_(std::make_shared<UIAElement>(sender, automation_));
            } catch (const std::exception& e) {
                spdlog::error("[UIA] Win 结构变化回调异常: {}", e.what());
            } catch (...) {
                spdlog::error("[UIA] Win 结构变化回调未知异常");
            }
        }
        return S_OK;
    }
private:
    CComPtr<IUIAutomation> automation_;
    wingman::IUIAManager::UIAEventCallback callback_;
    LONG refCount_;
};

class UIAManager : public IUIAManager {
public:
    ~UIAManager() {
        // 兜底：未显式 shutdown() 的路径（如异常）也清理监听器并 join 事件线程，
        // 避免 joinable 线程析构触发 std::terminate。
        if (initialized_) shutdown();
    }

    bool initialize() override {
        if (initialized_) return true;
        HRESULT hr = automation_.CoCreateInstance(__uuidof(CUIAutomation8));
        if (FAILED(hr)) {
            spdlog::error("Failed to create UIAutomation: 0x{:X}", hr);
            return false;
        }
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        // 先清理所有事件监听器（必须在事件线程停止前，cleanupListener 依赖事件线程）
        std::map<uint64_t, std::unique_ptr<WinUiaEventListener>> toCleanup;
        {
            std::lock_guard<std::mutex> lk(listenersMutex_);
            toCleanup.swap(listeners_);
        }
        for (auto& kv : toCleanup) {
            if (kv.second) cleanupListener(*kv.second);
        }
        // 停止事件线程
        if (eventThread_.joinable()) {
            {
                std::lock_guard<std::mutex> lk(eventMutex_);
                eventStop_ = true;
            }
            eventCv_.notify_all();
            eventThread_.join();
        }
        automation_.Release();
        initialized_ = false;
    }

    std::shared_ptr<IUIAElement> getFocusedElement() override {
        if (!initialized_) return nullptr;
        CComPtr<IUIAutomationElement> focused;
        if (FAILED(automation_->GetFocusedElement(&focused))) return nullptr;
        return std::make_shared<UIAElement>(focused, automation_);
    }

    std::shared_ptr<IUIAElement> getElementFromPoint(const Point& point) override {
        if (!initialized_) return nullptr;
        CComPtr<IUIAutomationElement> element;
        POINT pt = { point.x, point.y };
        if (FAILED(automation_->ElementFromPoint(pt, &element))) return nullptr;
        return std::make_shared<UIAElement>(element, automation_);
    }

    std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) override {
        if (!initialized_) return nullptr;

        CComPtr<IUIAutomationElement> root;
        if (FAILED(automation_->GetRootElement(&root)) || !root) return nullptr;

        return findElementRecursive(root, selector);
    }

    // Plan 6: 新增方法实现
    std::shared_ptr<IUIAElement> getElementFromWindow(uint64_t hwnd) override {
        if (!initialized_) return nullptr;
        CComPtr<IUIAutomationElement> element;
        if (FAILED(automation_->ElementFromHandle(reinterpret_cast<UIA_HWND>(hwnd), &element))) return nullptr;
        return std::make_shared<UIAElement>(element, automation_);
    }

    std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role) override {
        std::vector<std::shared_ptr<IUIAElement>> results;
        if (!initialized_ || role == UIARole::Unknown) return results;

        CComPtr<IUIAutomationElement> root;
        if (FAILED(automation_->GetRootElement(&root)) || !root) return results;

        // UIARole -> ControlType 映射
        CONTROLTYPEID controlType = 0;
        switch (role) {
            case UIARole::Button: controlType = UIA_ButtonControlTypeId; break;
            case UIARole::CheckBox: controlType = UIA_CheckBoxControlTypeId; break;
            case UIARole::ComboBox: controlType = UIA_ComboBoxControlTypeId; break;
            case UIARole::TextBox: controlType = UIA_EditControlTypeId; break;
            case UIARole::Window: controlType = UIA_WindowControlTypeId; break;
            case UIARole::RadioButton: controlType = UIA_RadioButtonControlTypeId; break;
            default: return results;
        }

        CComPtr<IUIAutomationCondition> cond;
        if (FAILED(automation_->CreatePropertyCondition(UIA_ControlTypeIdPropertyId, controlType, &cond)) || !cond) return results;

        CComPtr<IUIAutomationElementArray> array;
        if (FAILED(root->FindAll(TreeScope_Descendants, cond, &array)) || !array) return results;

        int count = 0;
        if (FAILED(array->get_Length(&count))) return results;
        for (int i = 0; i < count; ++i) {
            CComPtr<IUIAutomationElement> elem;
            if (SUCCEEDED(array->GetElement(i, &elem)) && elem) {
                results.push_back(std::make_shared<UIAElement>(elem, automation_));
            }
        }
        return results;
    }

    std::shared_ptr<IUIAElement> waitForElement(const UIASelector& selector, int timeoutMs) override {
        if (!initialized_) return nullptr;
        const int stepMs = 100;
        int elapsed = 0;
        while (elapsed < timeoutMs) {
            auto result = findElement(selector);
            if (result) return result;
            std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
            elapsed += stepMs;
        }
        return findElement(selector); // 超时前最后查一次
    }

    // Plan 6 Phase 2: 事件监听实现
    uint64_t addPropertyChangedListener(const UIASelector& selector, UIAEventCallback cb) override {
        if (!initialized_) return 0;
        CComPtr<IUIAutomationElement> scope = resolveScopeElement(selector);
        if (!scope) return 0;

        auto listener = std::make_unique<WinUiaEventListener>();
        listener->scopeElement = scope;
        listener->structureChanged = false;
        // 监听常用属性：名称/值/启用/可见
        PROPERTYID propIds[] = { UIA_NamePropertyId, UIA_ValuePropertyId,
                                 UIA_IsEnabledPropertyId, UIA_IsOffscreenPropertyId };
        auto* handler = new WinUiaPropertyChangedHandler(automation_, std::move(cb));
        listener->propHandler = handler;

        HRESULT hrAdd = E_FAIL;
        runOnEventThread([this, &scope, handler, &hrAdd, &propIds]() {
            hrAdd = automation_->AddPropertyChangedEventHandler(scope, TreeScope_Subtree, nullptr, handler, propIds, 4);
        });
        if (FAILED(hrAdd)) {
            handler->Release();  // 清理新建引用
            spdlog::warn("[UIA] AddPropertyChangedEventHandler 失败: 0x{:X}", static_cast<unsigned long>(hrAdd));
            return 0;
        }

        std::lock_guard<std::mutex> lk(listenersMutex_);
        uint64_t id = nextListenerId_++;
        listeners_[id] = std::move(listener);
        return id;
    }

    uint64_t addStructureChangedListener(const UIASelector& selector, UIAEventCallback cb) override {
        if (!initialized_) return 0;
        CComPtr<IUIAutomationElement> scope = resolveScopeElement(selector);
        if (!scope) return 0;

        auto listener = std::make_unique<WinUiaEventListener>();
        listener->scopeElement = scope;
        listener->structureChanged = true;
        auto* handler = new WinUiaStructureChangedHandler(automation_, std::move(cb));
        listener->structHandler = handler;

        HRESULT hrAdd = E_FAIL;
        runOnEventThread([this, &scope, handler, &hrAdd]() {
            hrAdd = automation_->AddStructureChangedEventHandler(scope, TreeScope_Subtree, nullptr, handler);
        });
        if (FAILED(hrAdd)) {
            handler->Release();
            spdlog::warn("[UIA] AddStructureChangedEventHandler 失败: 0x{:X}", static_cast<unsigned long>(hrAdd));
            return 0;
        }

        std::lock_guard<std::mutex> lk(listenersMutex_);
        uint64_t id = nextListenerId_++;
        listeners_[id] = std::move(listener);
        return id;
    }

    bool removeEventListener(uint64_t listenerId) override {
        std::unique_ptr<WinUiaEventListener> listener;
        {
            std::lock_guard<std::mutex> lk(listenersMutex_);
            auto it = listeners_.find(listenerId);
            if (it == listeners_.end()) return false;
            listener = std::move(it->second);
            listeners_.erase(it);
        }
        if (listener) cleanupListener(*listener);
        return true;
    }

    std::string getBackendName() const override { return "Windows UIAutomation"; }
    bool isAvailable() const override { return initialized_; }

private:
    CComPtr<IUIAutomation> automation_;
    bool initialized_ = false;

    // Plan 6 Phase 2: 事件监听
    struct WinUiaEventListener {
        CComPtr<IUIAutomationElement> scopeElement;
        WinUiaPropertyChangedHandler* propHandler = nullptr;       // 裸指针，手动 Release
        WinUiaStructureChangedHandler* structHandler = nullptr;
        bool structureChanged = false;
    };
    uint64_t nextListenerId_ = 1;
    std::map<uint64_t, std::unique_ptr<WinUiaEventListener>> listeners_;
    std::mutex listenersMutex_;

    // 专用事件线程（MTA，COM Add/RemoveEventHandler 必须在 COM 初始化线程执行）
    std::thread eventThread_;
    std::mutex eventMutex_;
    std::condition_variable eventCv_;
    std::queue<std::function<void()>> eventTasks_;
    std::atomic<bool> eventStop_{false};
    std::once_flag eventThreadStartFlag_;

    void ensureEventThread() {
        std::call_once(eventThreadStartFlag_, [this]() {
            eventThread_ = std::thread([this]() {
                // UIA 事件在 MTA 注册，回调从 UIA 内部 RPC 线程触发，无需用户消息泵
                CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lk(eventMutex_);
                        eventCv_.wait(lk, [this]{ return eventStop_.load() || !eventTasks_.empty(); });
                        if (eventStop_.load() && eventTasks_.empty()) break;
                        if (!eventTasks_.empty()) { task = std::move(eventTasks_.front()); eventTasks_.pop(); }
                    }
                    if (task) {
                        try { task(); } catch (const std::exception& e) {
                            spdlog::error("[UIA] Win 事件线程任务异常: {}", e.what());
                        } catch (...) {
                            spdlog::error("[UIA] Win 事件线程任务未知异常");
                        }
                    }
                }
                CoUninitialize();
            });
        });
    }

    // 在事件线程同步执行一个 COM 操作并等待完成。
    // 注意：本函数同步等待 task（doneCv.wait），故调用方按引用捕获的局部变量
    // （scope/handler/propIds/hrAdd 等）在 task 执行期间有效。禁止改为异步，否则引用悬垂。
    void runOnEventThread(const std::function<void()>& task) {
        ensureEventThread();
        std::mutex doneMutex;
        std::condition_variable doneCv;
        bool done = false;
        {
            std::lock_guard<std::mutex> lk(eventMutex_);
            eventTasks_.push([&task, &doneMutex, &doneCv, &done]() {
                task();
                std::lock_guard<std::mutex> dk(doneMutex);
                done = true;
                doneCv.notify_one();
            });
        }
        eventCv_.notify_one();
        std::unique_lock<std::mutex> lk(doneMutex);
        doneCv.wait(lk, [&]{ return done; });
    }

    // scope 解析：selector 全空→root；非空→findElement 匹配元素；失败 nullptr
    CComPtr<IUIAutomationElement> resolveScopeElement(const UIASelector& selector) {
        bool selectorEmpty = selector.name.empty() && selector.id.empty() &&
                             selector.className.empty() && selector.role == UIARole::Unknown &&
                             selector.text.empty();
        if (selectorEmpty) {
            CComPtr<IUIAutomationElement> root;
            if (FAILED(automation_->GetRootElement(&root)) || !root) return nullptr;
            return root;
        }
        auto scope = findElement(selector);
        if (!scope) return nullptr;
        return static_cast<UIAElement*>(scope.get())->elementRef();
    }

    void cleanupListener(WinUiaEventListener& l) {
        runOnEventThread([this, &l]() {
            if (l.structureChanged && l.structHandler && l.scopeElement) {
                HRESULT hr = automation_->RemoveStructureChangedEventHandler(l.scopeElement, l.structHandler);
                if (FAILED(hr)) spdlog::warn("[UIA] RemoveStructureChangedEventHandler 失败: 0x{:X}", static_cast<unsigned long>(hr));
            } else if (l.propHandler && l.scopeElement) {
                HRESULT hr = automation_->RemovePropertyChangedEventHandler(l.scopeElement, l.propHandler);
                if (FAILED(hr)) spdlog::warn("[UIA] RemovePropertyChangedEventHandler 失败: 0x{:X}", static_cast<unsigned long>(hr));
            }
        });
        // RemoveEventHandler 后 UIA 释放 handler 引用，再 Release 我们的新建引用（refCount→0 delete）
        if (l.propHandler) { l.propHandler->Release(); l.propHandler = nullptr; }
        if (l.structHandler) { l.structHandler->Release(); l.structHandler = nullptr; }
    }

    std::shared_ptr<IUIAElement> findElementRecursive(CComPtr<IUIAutomationElement> parent, const UIASelector& selector) {
        if (!parent) return nullptr;

        // Check if current element matches
        auto current = std::make_shared<UIAElement>(parent, automation_);
        if (selector.matches(current->getInfo())) {
            return current;
        }

        // Recursively find child elements
        CComPtr<IUIAutomationTreeWalker> walker;
        if (FAILED(automation_->get_ControlViewWalker(&walker)) || !walker) return nullptr;

        CComPtr<IUIAutomationElement> child;
        if (FAILED(walker->GetFirstChildElement(parent, &child))) return nullptr;

        while (child) {
            auto result = findElementRecursive(child, selector);
            if (result) return result;

            CComPtr<IUIAutomationElement> next;
            if (FAILED(walker->GetNextSiblingElement(child, &next))) break;
            child = next;
        }

        return nullptr;
    }
};

} // namespace wingman

std::unique_ptr<wingman::IUIAManager> createUIAManager() {
    return std::make_unique<wingman::UIAManager>();
}

#endif // _WIN32
