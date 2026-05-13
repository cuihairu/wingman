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

#ifdef _WIN32

namespace wingman {

class UIAElement : public IUIAElement {
public:
    explicit UIAElement(CComPtr<IUIAutomationElement> element) : element_(element) {}

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
        CComVariant var;
        if (SUCCEEDED(element_->get_CurrentAutomationId(&var)) && var.vt == VT_BSTR) {
            CW2A converted(var.bstrVal);
            return std::string(converted);
        }
        return "";
    }

    std::string getClassName() const override {
        if (!element_) return "";
        CComVariant var;
        if (SUCCEEDED(element_->get_CurrentClassName(&var)) && var.vt == VT_BSTR) {
            CW2A converted(var.bstrVal);
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
            ToggleState desired = checked ? ToggleState_On : ToggleState_Off;
            return SUCCEEDED(togglePattern->SetToggleState(desired));
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

private:
    CComPtr<IUIAutomationElement> element_;

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

class UIAManager : public IUIAManager {
public:
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
        automation_.Release();
        initialized_ = false;
    }

    std::shared_ptr<IUIAElement> getFocusedElement() override {
        if (!initialized_) return nullptr;
        CComPtr<IUIAutomationElement> focused;
        if (FAILED(automation_->GetFocusedElement(&focused))) return nullptr;
        return std::make_shared<UIAElement>(focused);
    }

    std::shared_ptr<IUIAElement> getElementFromPoint(const Point& point) override {
        if (!initialized_) return nullptr;
        CComPtr<IUIAutomationElement> element;
        if (FAILED(automation_->ElementFromPoint(point.x, point.y, &element))) return nullptr;
        return std::make_shared<UIAElement>(element);
    }

    std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) override {
        if (!initialized_) return nullptr;

        CComPtr<IUIAutomationElement> root;
        if (FAILED(automation_->GetRootElement(&root)) || !root) return nullptr;

        return findElementRecursive(root, selector);
    }

    std::string getBackendName() const override { return "Windows UIAutomation"; }
    bool isAvailable() const override { return initialized_; }

private:
    CComPtr<IUIAutomation> automation_;
    bool initialized_ = false;

    std::shared_ptr<IUIAElement> findElementRecursive(CComPtr<IUIAutomationElement> parent, const UIASelector& selector) {
        if (!parent) return nullptr;

        // 检查当前元素是否匹配
        auto current = std::make_shared<UIAElement>(parent);
        if (selector.matches(current->getInfo())) {
            return current;
        }

        // 递归查找子元素
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

std::unique_ptr<IUIAManager> createUIAManager() {
    return std::make_unique<UIAManager>();
}

} // namespace wingman

#endif // _WIN32
