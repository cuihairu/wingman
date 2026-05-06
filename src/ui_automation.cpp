#include "wingman/ui_automation.hpp"
#include "wingman/window.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace wingman {

// ========== UIAutomationElement::Impl ==========

struct UIAutomationElement::Impl {
    IUIAutomationElement* element = nullptr;
    bool own = false;  // 是否拥有所有权（需要释放）

    Impl() : element(nullptr), own(false) {}

    ~Impl() {
        if (own && element) {
            element->Release();
            element = nullptr;
        }
    }

    // 辅助：获取 Name 属性
    std::string getName() const {
        if (!element) return "";
        BSTR bstr = nullptr;
        if (SUCCEEDED(element->get_CurrentName(&bstr)) && bstr) {
            int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string result(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, bstr, -1, &result[0], len, nullptr, nullptr);
                SysFreeString(bstr);
                return result;
            }
            SysFreeString(bstr);
        }
        return "";
    }

    // 辅助：获取 ClassName 属性
    std::string getClassName() const {
        if (!element) return "";
        BSTR bstr = nullptr;
        if (SUCCEEDED(element->get_CurrentClassName(&bstr)) && bstr) {
            int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string result(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, bstr, -1, &result[0], len, nullptr, nullptr);
                SysFreeString(bstr);
                return result;
            }
            SysFreeString(bstr);
        }
        return "";
    }

    // 辅助：获取 AutomationId 属性
    std::string getAutomationId() const {
        if (!element) return "";
        BSTR bstr = nullptr;
        if (SUCCEEDED(element->get_CurrentAutomationId(&bstr)) && bstr) {
            int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string result(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, bstr, -1, &result[0], len, nullptr, nullptr);
                SysFreeString(bstr);
                return result;
            }
            SysFreeString(bstr);
        }
        return "";
    }

    // 辅助：获取 ControlType
    CONTROLTYPEID getControlType() const {
        if (!element) return 0;
        CONTROLTYPEID controlType = 0;
        element->get_CurrentControlType(&controlType);
        return controlType;
    }

    // 辅助：获取边界矩形
    Rect getBounds() const {
        Rect result = {};
        if (!element) return result;

        RECT rect = {};
        if (SUCCEEDED(element->get_CurrentBoundingRectangle(&rect))) {
            result.x = rect.left;
            result.y = rect.top;
            result.width = rect.right - rect.left;
            result.height = rect.bottom - rect.top;
        }
        return result;
    }

    // 辅助：获取窗口句柄
    HWND getHandle() const {
        if (!element) return nullptr;
        HWND hwnd = nullptr;
        element->get_CurrentNativeWindowHandle(&hwnd);
        return hwnd;
    }
};

// ========== UIAutomationElement ==========

UIAutomationElement::UIAutomationElement() : impl(std::make_unique<Impl>()) {}

UIAutomationElement::UIAutomationElement(Impl* p) : impl(std::unique_ptr<Impl>(p)) {}

UIAutomationElement::~UIAutomationElement() = default;

bool UIAutomationElement::isValid() const {
    return impl->element != nullptr;
}

bool UIAutomationElement::isEnabled() const {
    if (!impl->element) return false;
    BOOL enabled = FALSE;
    impl->element->get_CurrentIsEnabled(&enabled);
    return enabled != FALSE;
}

bool UIAutomationElement::isVisible() const {
    if (!impl->element) return false;
    BOOL visible = FALSE;
    impl->element->get_CurrentIsOffscreen(&visible);
    return !visible;  // IsOffscreen = TRUE 意味着不可见
}

std::string UIAutomationElement::getName() const {
    return impl->getName();
}

std::string UIAutomationElement::getClassName() const {
    return impl->getClassName();
}

std::string UIAutomationElement::getAutomationId() const {
    return impl->getAutomationId();
}

Rect UIAutomationElement::getBounds() const {
    return impl->getBounds();
}

UIElementInfo UIAutomationElement::getInfo() const {
    UIElementInfo info;
    if (!impl->element) return info;

    info.name = impl->getName();
    info.className = impl->getClassName();
    info.automationId = impl->getAutomationId();
    info.bounds = impl->getBounds();
    info.isEnabled = isEnabled();
    info.isVisible = isVisible();
    info.handle = reinterpret_cast<intptr_t>(impl->getHandle());

    // 控制类型名称
    CONTROLTYPEID controlType = impl->getControlType();
    if (controlType == UIA_ButtonControlTypeId) info.controlType = "Button";
    else if (controlType == UIA_EditControlTypeId) info.controlType = "Edit";
    else if (controlType == UIA_TextControlTypeId) info.controlType = "Text";
    else if (controlType == UIA_ComboBoxControlTypeId) info.controlType = "ComboBox";
    else if (controlType == UIA_ListControlTypeId) info.controlType = "List";
    else if (controlType == UIA_ListItemControlTypeId) info.controlType = "ListItem";
    else if (controlType == UIA_CheckBoxControlTypeId) info.controlType = "CheckBox";
    else if (controlType == UIA_RadioButtonControlTypeId) info.controlType = "RadioButton";
    else if (controlType == UIA_WindowControlTypeId) info.controlType = "Window";
    else if (controlType == UIA_MenuControlTypeId) info.controlType = "Menu";
    else if (controlType == UIA_MenuItemControlTypeId) info.controlType = "MenuItem";
    else if (controlType == UIA_TabControlTypeId) info.controlType = "Tab";
    else if (controlType == UIA_TabItemControlTypeId) info.controlType = "TabItem";
    else if (controlType == UIA_TreeControlTypeId) info.controlType = "Tree";
    else if (controlType == UIA_TreeItemControlTypeId) info.controlType = "TreeItem";
    else if (controlType == UIA_ScrollBarControlTypeId) info.controlType = "ScrollBar";
    else if (controlType == UIA_SliderControlTypeId) info.controlType = "Slider";
    else if (controlType == UIA_ProgressBarControlTypeId) info.controlType = "ProgressBar";
    else if (controlType == UIA_GroupControlTypeId) info.controlType = "Group";
    else if (controlType == UIA_PaneControlTypeId) info.controlType = "Pane";
    else if (controlType == UIA_ToolBarControlTypeId) info.controlType = "ToolBar";
    else if (controlType == UIA_StatusBarControlTypeId) info.controlType = "StatusBar";
    else if (controlType == UIA_ToolTipControlTypeId) info.controlType = "ToolTip";
    else if (controlType == UIA_HyperlinkControlTypeId) info.controlType = "Hyperlink";
    else if (controlType == UIA_ImageControlTypeId) info.controlType = "Image";
    else if (controlType == UIA_DataGridControlTypeId) info.controlType = "DataGrid";
    else if (controlType == UIA_DataItemControlTypeId) info.controlType = "DataItem";
    else if (controlType == UIA_DocumentControlTypeId) info.controlType = "Document";
    else if (controlType == UIA_SpinnerControlTypeId) info.controlType = "Spinner";
    else if (controlType == UIA_CalendarControlTypeId) info.controlType = "Calendar";
    else info.controlType = "Unknown";

    return info;
}

bool UIAutomationElement::click() {
    if (!impl->element) return false;

    // 尝试 Invoke Pattern
    IUIAutomationInvokePattern* invokePattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_InvokePatternId, __uuidof(IUIAutomationInvokePattern), reinterpret_cast<void**>(&invokePattern))) && invokePattern) {
        HRESULT hr = invokePattern->Invoke();
        invokePattern->Release();
        return SUCCEEDED(hr);
    }

    // 降级到鼠标点击
    Rect bounds = impl->getBounds();
    if (bounds.width > 0 && bounds.height > 0) {
        int centerX = bounds.x + bounds.width / 2;
        int centerY = bounds.y + bounds.height / 2;
        return Input::click(centerX, centerY);
    }

    return false;
}

bool UIAutomationElement::rightClick() {
    Rect bounds = impl->getBounds();
    if (bounds.width > 0 && bounds.height > 0) {
        int centerX = bounds.x + bounds.width / 2;
        int centerY = bounds.y + bounds.height / 2;
        return Input::rightClick(centerX, centerY);
    }
    return false;
}

bool UIAutomationElement::doubleClick() {
    Rect bounds = impl->getBounds();
    if (bounds.width > 0 && bounds.height > 0) {
        int centerX = bounds.x + bounds.width / 2;
        int centerY = bounds.y + bounds.height / 2;
        return Input::doubleClick(centerX, centerY);
    }
    return false;
}

bool UIAutomationElement::focus() {
    if (!impl->element) return false;

    IUIAutomationLegacyIAccessiblePattern* legacyPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, __uuidof(IUIAutomationLegacyIAccessiblePattern), reinterpret_cast<void**>(&legacyPattern))) && legacyPattern) {
        HRESULT hr = legacyPattern->DoDefaultAction();
        legacyPattern->Release();
        return SUCCEEDED(hr);
    }

    // 尝试设置焦点
    return SUCCEEDED(impl->element->SetFocus());
}

std::string UIAutomationElement::getValue() const {
    if (!impl->element) return "";

    IUIAutomationValuePattern* valuePattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern), reinterpret_cast<void**>(&valuePattern))) && valuePattern) {
        BSTR bstr = nullptr;
        if (SUCCEEDED(valuePattern->get_CurrentValue(&bstr)) && bstr) {
            int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string result(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, bstr, -1, &result[0], len, nullptr, nullptr);
                SysFreeString(bstr);
                valuePattern->Release();
                return result;
            }
            SysFreeString(bstr);
        }
        valuePattern->Release();
    }

    return "";
}

bool UIAutomationElement::setValue(const std::string& value) {
    if (!impl->element) return false;

    IUIAutomationValuePattern* valuePattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern), reinterpret_cast<void**>(&valuePattern))) && valuePattern) {
        int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (len > 0) {
            BSTR bstr = SysAllocStringLen(nullptr, len);
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, bstr, len);
            HRESULT hr = valuePattern->SetValue(bstr);
            SysFreeString(bstr);
            valuePattern->Release();
            return SUCCEEDED(hr);
        }
        valuePattern->Release();
    }

    return false;
}

std::vector<std::string> UIAutomationElement::getSelectionItems() const {
    std::vector<std::string> items;

    if (!impl->element) return items;

    // 尝试 Selection Pattern 获取选中的项
    IUIAutomationSelectionPattern* selectionPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_SelectionPatternId, __uuidof(IUIAutomationSelectionPattern), reinterpret_cast<void**>(&selectionPattern))) && selectionPattern) {
        IUIAutomationElementArray* selection = nullptr;
        if (SUCCEEDED(selectionPattern->GetCurrentSelection(&selection)) && selection) {
            int count = 0;
            selection->get_Length(&count);
            for (int i = 0; i < count; i++) {
                IUIAutomationElement* item = nullptr;
                if (SUCCEEDED(selection->GetElement(i, &item)) && item) {
                    BSTR bstr = nullptr;
                    if (SUCCEEDED(item->get_CurrentName(&bstr)) && bstr) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
                        if (len > 0) {
                            std::string name(len - 1, 0);
                            WideCharToMultiByte(CP_UTF8, 0, bstr, -1, &name[0], len, nullptr, nullptr);
                            items.push_back(name);
                        }
                        SysFreeString(bstr);
                    }
                    item->Release();
                }
            }
            selection->Release();
        }
        selectionPattern->Release();
    }

    // 如果没有选中项，尝试获取所有可选项
    if (items.empty()) {
        // 尝试从子元素中获取
        auto children = getChildren();
        for (auto& child : children) {
            std::string name = child->getName();
            if (!name.empty()) {
                items.push_back(name);
            }
        }
    }

    return items;
}

std::shared_ptr<UIAutomationElement> UIAutomationElement::getParent() {
    if (!impl->element) return nullptr;

    IUIAutomationTreeWalker* walker = nullptr;
    if (FAILED(uia().impl->automation->get_ControlViewWalker(&walker)) || !walker) {
        return nullptr;
    }

    IUIAutomationElement* parent = nullptr;
    walker->GetParentElement(impl->element, &parent);
    walker->Release();

    if (parent) {
        auto elemImpl = new Impl();
        elemImpl->element = parent;
        elemImpl->own = true;
        return std::make_shared<UIAutomationElement>(elemImpl);
    }

    return nullptr;
}

std::vector<std::shared_ptr<UIAutomationElement>> UIAutomationElement::getChildren() {
    std::vector<std::shared_ptr<UIAutomationElement>> children;

    if (!impl->element) return children;

    IUIAutomationTreeWalker* walker = nullptr;
    if (FAILED(uia().impl->automation->get_ControlViewWalker(&walker)) || !walker) {
        return children;
    }

    IUIAutomationElement* child = nullptr;
    walker->GetFirstChildElement(impl->element, &child);

    while (child) {
        auto elemImpl = new Impl();
        elemImpl->element = child;
        elemImpl->own = true;
        children.push_back(std::make_shared<UIAutomationElement>(elemImpl));

        walker->GetNextSiblingElement(child, &child);
    }

    walker->Release();
    return children;
}

// ========== UIAutomation::Impl ==========

struct UIAutomation::Impl {
    IUIAutomation* automation = nullptr;
    bool initialized = false;

    ~Impl() {
        if (automation) {
            automation->Release();
            automation = nullptr;
        }
        CoUninitialize();
    }
};

// ========== UIAutomation ==========

UIAutomation* UIAutomation::instance_ = nullptr;

UIAutomation::UIAutomation() : impl(std::make_unique<Impl>()) {}

UIAutomation::~UIAutomation() {
    if (this == instance_) {
        cleanup();
        instance_ = nullptr;
    }
}

bool UIAutomation::initialize() {
    if (impl->initialized) return true;

    // 初始化 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::error("[UIA] Failed to initialize COM: 0x{:X}", hr);
        return false;
    }

    // 创建 IUIAutomation 对象
    hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_ALL,
                          __uuidof(IUIAutomation), reinterpret_cast<void**>(&impl->automation));
    if (FAILED(hr)) {
        spdlog::error("[UIA] Failed to create CUIAutomation: 0x{:X}", hr);
        CoUninitialize();
        return false;
    }

    impl->initialized = true;
    spdlog::info("[UIA] UI Automation initialized");
    return true;
}

void UIAutomation::cleanup() {
    if (impl) {
        impl.reset();
    }
    CoUninitialize();
}

std::shared_ptr<UIAutomationElement> UIAutomation::fromWindow(HWND hwnd) {
    if (!initialize()) return nullptr;

    IUIAutomationElement* element = nullptr;
    HRESULT hr = impl->automation->ElementFromHandle(hwnd, &element);
    if (FAILED(hr) || !element) {
        return nullptr;
    }

    auto elemImpl = new UIAutomationElement::Impl();
    elemImpl->element = element;
    elemImpl->own = true;
    return std::make_shared<UIAutomationElement>(elemImpl);
}

std::shared_ptr<UIAutomationElement> UIAutomation::fromForegroundWindow() {
    HWND hwnd = Window::getForeground();
    if (!hwnd) return nullptr;
    return fromWindow(hwnd);
}

std::shared_ptr<UIAutomationElement> UIAutomation::fromPoint(int x, int y) {
    if (!initialize()) return nullptr;

    IUIAutomationElement* element = nullptr;
    POINT pt = { x, y };
    HRESULT hr = impl->automation->ElementFromPoint(pt, &element);
    if (FAILED(hr) || !element) {
        return nullptr;
    }

    auto elemImpl = new UIAutomationElement::Impl();
    elemImpl->element = element;
    elemImpl->own = true;
    return std::make_shared<UIAutomationElement>(elemImpl);
}

// 便捷查找方法
std::shared_ptr<UIAutomationElement> UIAutomation::findByName(const std::string& name) {
    auto root = fromForegroundWindow();
    if (!root) return nullptr;

    // 在子元素中查找
    auto children = root->getChildren();
    for (auto& child : children) {
        if (child->getName() == name) {
            return child;
        }
    }

    return nullptr;
}

std::shared_ptr<UIAutomationElement> UIAutomation::findById(const std::string& automationId) {
    auto root = fromForegroundWindow();
    if (!root) return nullptr;

    auto children = root->getChildren();
    for (auto& child : children) {
        if (child->getAutomationId() == automationId) {
            return child;
        }
    }

    return nullptr;
}

std::shared_ptr<UIAutomationElement> UIAutomation::findButton(const std::string& name) {
    auto root = fromForegroundWindow();
    if (!root) return nullptr;

    auto children = root->getChildren();
    for (auto& child : children) {
        if (child->impl->getControlType() == UIA_ButtonControlTypeId) {
            std::string childName = child->getName();
            if (childName.find(name) != std::string::npos) {
                return child;
            }
        }
    }

    return nullptr;
}

std::shared_ptr<UIAutomationElement> UIAutomation::findEdit(const std::string& name) {
    auto root = fromForegroundWindow();
    if (!root) return nullptr;

    auto children = root->getChildren();
    for (auto& child : children) {
        CONTROLTYPEID controlType = child->impl->getControlType();
        if (controlType == UIA_EditControlTypeId || controlType == UIA_DocumentControlTypeId) {
            std::string childName = child->getName();
            if (childName.find(name) != std::string::npos) {
                return child;
            }
        }
    }

    return nullptr;
}

std::shared_ptr<UIAutomationElement> UIAutomation::findText(const std::string& name) {
    auto root = fromForegroundWindow();
    if (!root) return nullptr;

    auto children = root->getChildren();
    for (auto& child : children) {
        if (child->impl->getControlType() == UIA_TextControlTypeId) {
            std::string childName = child->getName();
            if (childName.find(name) != std::string::npos) {
                return child;
            }
        }
    }

    return nullptr;
}

std::shared_ptr<UIAutomationElement> UIAutomation::waitFor(const UIACondition& condition, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count() < timeoutMs) {

        auto element = find(condition);
        if (element && element->isValid()) {
            return element;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return nullptr;
}

std::shared_ptr<UIAutomationElement> UIAutomation::waitForName(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count() < timeoutMs) {

        auto element = findByName(name);
        if (element && element->isValid()) {
            return element;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return nullptr;
}

// ========== UIAutomationElement: 扩展功能 ==========

std::string UIAutomationElement::getSelection() const {
    if (!impl->element) return "";

    auto items = getSelectionItems();
    if (!items.empty()) {
        return items[0];
    }
    return "";
}

bool UIAutomationElement::selectItem(const std::string& item) {
    if (!impl->element) return false;

    // 尝试 Selection Pattern
    IUIAutomationSelectionPattern* selectionPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_SelectionPatternId, __uuidof(IUIAutomationSelectionPattern), reinterpret_cast<void**>(&selectionPattern))) && selectionPattern) {
        // 首先在子元素中查找匹配项
        auto children = getChildren();
        for (auto& child : children) {
            if (child->getName() == item) {
                IUIAutomationLegacyIAccessiblePattern* legacyPattern = nullptr;
                if (SUCCEEDED(child->impl->element->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, __uuidof(IUIAutomationLegacyIAccessiblePattern), reinterpret_cast<void**>(&legacyPattern))) && legacyPattern) {
                    // 使用 DoDefaultAction 来选择（通常对应单击选择）
                    HRESULT hr = legacyPattern->DoDefaultAction();
                    legacyPattern->Release();
                    selectionPattern->Release();
                    return SUCCEEDED(hr);
                }
            }
        }
        selectionPattern->Release();
    }

    // 尝试 SelectionItem Pattern（如果元素本身支持）
    IUIAutomationSelectionItemPattern* selectionItemPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_SelectionItemPatternId, __uuidof(IUIAutomationSelectionItemPattern), reinterpret_cast<void**>(&selectionItemPattern))) && selectionItemPattern) {
        HRESULT hr = selectionItemPattern->Select();
        selectionItemPattern->Release();
        return SUCCEEDED(hr);
    }

    return false;
}

bool UIAutomationElement::expand() {
    if (!impl->element) return false;

    IUIAutomationExpandCollapsePattern* expandPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, __uuidof(IUIAutomationExpandCollapsePattern), reinterpret_cast<void**>(&expandPattern))) && expandPattern) {
        HRESULT hr = expandPattern->Expand();
        expandPattern->Release();
        return SUCCEEDED(hr);
    }

    return false;
}

bool UIAutomationElement::collapse() {
    if (!impl->element) return false;

    IUIAutomationExpandCollapsePattern* expandPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, __uuidof(IUIAutomationExpandCollapsePattern), reinterpret_cast<void**>(&expandPattern))) && expandPattern) {
        HRESULT hr = expandPattern->Collapse();
        expandPattern->Release();
        return SUCCEEDED(hr);
    }

    return false;
}

bool UIAutomationElement::isExpanded() const {
    if (!impl->element) return false;

    IUIAutomationExpandCollapsePattern* expandPattern = nullptr;
    if (SUCCEEDED(impl->element->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, __uuidof(IUIAutomationExpandCollapsePattern), reinterpret_cast<void**>(&expandPattern))) && expandPattern) {
        ExpandCollapseState state;
        if (SUCCEEDED(expandPattern->get_CurrentExpandCollapseState(&state))) {
            expandPattern->Release();
            return state == ExpandCollapseState_Expanded;
        }
        expandPattern->Release();
    }

    return false;
}

// 深度优先搜索元素
static std::shared_ptr<UIAutomationElement> findElementRecursive(
    const std::shared_ptr<UIAutomationElement>& root,
    const std::function<bool(const UIElementInfo&)>& predicate,
    bool depthFirst = true) {

    if (!root || !root->isValid()) return nullptr;

    // 检查当前元素
    UIElementInfo info = root->getInfo();
    if (predicate(info)) {
        return root;
    }

    auto children = root->getChildren();
    for (auto& child : children) {
        auto result = findElementRecursive(child, predicate, depthFirst);
        if (result) return result;
    }

    return nullptr;
}

// 查找所有匹配元素
static std::vector<std::shared_ptr<UIAutomationElement>> findAllElementsRecursive(
    const std::shared_ptr<UIAutomationElement>& root,
    const std::function<bool(const UIElementInfo&)>& predicate) {

    std::vector<std::shared_ptr<UIAutomationElement>> results;

    if (!root || !root->isValid()) return results;

    // 检查当前元素
    UIElementInfo info = root->getInfo();
    if (predicate(info)) {
        results.push_back(root);
    }

    // 递归搜索子元素
    auto children = root->getChildren();
    for (auto& child : children) {
        auto childResults = findAllElementsRecursive(child, predicate);
        results.insert(results.end(), childResults.begin(), childResults.end());
    }

    return results;
}

std::shared_ptr<UIAutomationElement> UIAutomationElement::findFirst(const UIACondition& condition) {
    // 简化实现：直接使用名称查找
    // 完整实现需要解析 condition 的条件
    if (!condition.impl) return nullptr;

    // 这里简化为只查找直接子元素
    auto children = getChildren();
    for (auto& child : children) {
        if (child->isValid()) {
            return child;  // 返回第一个有效子元素
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<UIAutomationElement>> UIAutomationElement::findAll(const UIACondition& condition) {
    // 简化实现
    return getChildren();
}

std::vector<std::shared_ptr<UIAutomationElement>> UIAutomationElement::findAllDescendants(const UIACondition& condition) {
    // 简化实现：返回所有后代
    std::vector<std::shared_ptr<UIAutomationElement>> results;

    std::function<void(std::shared_ptr<UIAutomationElement>)> collect;
    collect = [&results, &collect](const std::shared_ptr<UIAutomationElement>& elem) {
        if (!elem || !elem->isValid()) return;
        results.push_back(elem);
        auto children = elem->getChildren();
        for (auto& child : children) {
            collect(child);
        }
    };

    collect(shared_from_this());
    return results;
}

// 全局访问
UIAutomation& uia() {
    static UIAutomation instance;
    return instance;
}

} // namespace wingman
