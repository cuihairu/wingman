#include "wingman/ui_automation.hpp"
#include <spdlog/spdlog.h>
#include <memory>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>

namespace wingman {

class UIAElement : public IUIAElement {
public:
    explicit UIAElement(AXUIElementRef element) : element_(element) {
        if (element_) CFRetain(element_);
    }

    ~UIAElement() override {
        if (element_) CFRelease(element_);
    }

    std::string getName() const override {
        if (!element_) return "";
        CFStringRef name = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXTitleAttribute, (CFTypeRef*)&name) == kAXErrorSuccess && name) {
            std::string result = cfStringToStdString(name);
            CFRelease(name);
            return result;
        }
        return "";
    }

    std::string getId() const override { return ""; }

    std::string getClassName() const override {
        if (!element_) return "";
        CFStringRef role = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXRoleAttribute, (CFTypeRef*)&role) == kAXErrorSuccess && role) {
            std::string result = cfStringToStdString(role);
            CFRelease(role);
            return result;
        }
        return "";
    }

    UIARole getRole() const override {
        if (!element_) return UIARole::Unknown;
        CFStringRef role = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXRoleAttribute, (CFTypeRef*)&role) == kAXErrorSuccess && role) {
            UIARole result = roleToElementRole(role);
            CFRelease(role);
            return result;
        }
        return UIARole::Unknown;
    }

    Rect getBounds() const override {
        if (!element_) return Rect{};
        CFTypeRef positionRef = nullptr;
        CFTypeRef sizeRef = nullptr;

        if (AXUIElementCopyAttributeValue(element_, kAXPositionAttribute, &positionRef) == kAXErrorSuccess &&
            AXUIElementCopyAttributeValue(element_, kAXSizeAttribute, &sizeRef) == kAXErrorSuccess) {

            AXValueRef position = (AXValueRef)positionRef;
            AXValueRef size = (AXValueRef)sizeRef;

            CGPoint pt;
            CGSize sz;
            if (AXValueGetValue(position, kAXValueCGPointType, &pt) &&
                AXValueGetValue(size, kAXValueCGSizeType, &sz)) {

                CFRelease(positionRef);
                CFRelease(sizeRef);
                return Rect{static_cast<int>(pt.x), static_cast<int>(pt.y), static_cast<int>(sz.width), static_cast<int>(sz.height)};
            }
        }

        if (positionRef) CFRelease(positionRef);
        if (sizeRef) CFRelease(sizeRef);
        return Rect{};
    }

    bool isVisible() const override { return true; }

    bool isEnabled() const override {
        if (!element_) return false;
        CFBooleanRef enabled = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXEnabledAttribute, (CFTypeRef*)&enabled) == kAXErrorSuccess && enabled) {
            bool result = CFBooleanGetValue(enabled);
            CFRelease(enabled);
            return result;
        }
        return true;
    }

    std::string getText() const override {
        if (!element_) return "";
        CFStringRef value = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXValueAttribute, (CFTypeRef*)&value) == kAXErrorSuccess && value) {
            std::string result = cfStringToStdString(value);
            CFRelease(value);
            return result;
        }
        return getName();
    }

    bool setText(const std::string& text) override {
        if (!element_) return false;
        CFStringRef cfText = CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8);
        if (!cfText) return false;

        AXError err = AXUIElementSetAttributeValue(element_, kAXValueAttribute, cfText);
        CFRelease(cfText);
        return err == kAXErrorSuccess;
    }

    bool click() override {
        if (!element_) return false;
        AXError err = AXUIElementPerformAction(element_, kAXPressAction);
        return err == kAXErrorSuccess;
    }

    bool setFocus() override {
        if (!element_) return false;
        AXError err = AXUIElementSetAttributeValue(element_, kAXFocusedAttribute, kCFBooleanTrue);
        return err == kAXErrorSuccess;
    }

    bool hasFocus() const override {
        if (!element_) return false;
        CFBooleanRef focused = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXFocusedAttribute, (CFTypeRef*)&focused) == kAXErrorSuccess && focused) {
            bool result = CFBooleanGetValue(focused);
            CFRelease(focused);
            return result;
        }
        return false;
    }

    bool isChecked() const override {
        if (!element_) return false;
        CFNumberRef value = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXCheckedAttribute, (CFTypeRef*)&value) == kAXErrorSuccess && value) {
            int intValue = 0;
            CFNumberGetValue(value, kCFNumberIntType, &intValue);
            CFRelease(value);
            return intValue == 1;
        }
        return false;
    }

    bool setChecked(bool checked) override {
        if (!element_) return false;
        int value = checked ? 1 : 0;
        CFNumberRef cfValue = CFNumberCreate(nullptr, kCFNumberIntType, &value);
        if (!cfValue) return false;

        AXError err = AXUIElementSetAttributeValue(element_, kAXCheckedAttribute, cfValue);
        CFRelease(cfValue);
        return err == kAXErrorSuccess;
    }

    bool isValid() const override {
        if (!element_) return false;
        CFBooleanRef focused = nullptr;
        AXError err = AXUIElementCopyAttributeValue(element_, kAXFocusedAttribute, (CFTypeRef*)&focused);
        return err == kAXErrorSuccess || err == kAXErrorNoValue;
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
    AXUIElementRef element_;

    static std::string cfStringToStdString(CFStringRef cfStr) {
        if (!cfStr) return "";
        const char* ptr = CFStringGetCStringPtr(cfStr, kCFStringEncodingUTF8);
        if (ptr) return std::string(ptr);

        CFIndex length = CFStringGetLength(cfStr);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        char* buffer = new char[maxSize];
        if (CFStringGetCString(cfStr, buffer, maxSize, kCFStringEncodingUTF8)) {
            std::string result(buffer);
            delete[] buffer;
            return result;
        }
        delete[] buffer;
        return "";
    }

    static UIARole roleToElementRole(CFStringRef role) {
        std::string roleStr = cfStringToStdString(role);
        if (roleStr == kAXWindowRole) return UIARole::Window;
        if (roleStr == kAXButtonRole) return UIARole::Button;
        if (roleStr == kAXCheckBoxRole) return UIARole::CheckBox;
        if (roleStr == kAXRadioButtonRole) return UIARole::RadioButton;
        if (roleStr == kAXTextFieldRole) return UIARole::TextBox;
        return UIARole::Unknown;
    }
};

class UIAManager : public IUIAManager {
public:
    bool initialize() override {
        if (initialized_) return true;
        if (!AXIsProcessTrusted()) {
            spdlog::error("Accessibility permissions not granted");
            return false;
        }
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    std::shared_ptr<IUIAElement> getFocusedElement() override {
        if (!initialized_) return nullptr;

        AXUIElementRef focusedElement = nullptr;
        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        if (!systemWide) return nullptr;

        AXError err = AXUIElementCopyAttributeValue(systemWide, kAXFocusedUIElementAttribute, (CFTypeRef*)&focusedElement);
        CFRelease(systemWide);

        if (err != kAXErrorSuccess || !focusedElement) return nullptr;

        auto result = std::make_shared<UIAElement>(focusedElement);
        CFRelease(focusedElement);
        return result;
    }

    std::shared_ptr<IUIAElement> getElementFromPoint(const Point& point) override {
        if (!initialized_) return nullptr;

        AXUIElementRef element = nullptr;
        AXError err = AXUIElementCopyElementAtPosition(AXUIElementCreateSystemWide(), point.x, point.y, &element);
        if (err != kAXErrorSuccess || !element) return nullptr;

        auto result = std::make_shared<UIAElement>(element);
        CFRelease(element);
        return result;
    }

    std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) override {
        if (!initialized_) return nullptr;

        // 获取系统范围内的所有应用程序
        CFArrayRef applications = AXUIElementCreateApplicationList();
        if (!applications) return nullptr;

        CFIndex count = CFArrayGetCount(applications);
        for (CFIndex i = 0; i < count; ++i) {
            AXUIElementRef app = (AXUIElementRef)CFArrayGetValueAtIndex(applications, i);
            if (!app) continue;

            CFRetain(app);
            auto result = findElementRecursive(app, selector);
            CFRelease(app);

            if (result) {
                CFRelease(applications);
                return result;
            }
        }

        CFRelease(applications);
        return nullptr;
    }

    std::string getBackendName() const override { return "macOS Accessibility API"; }
    bool isAvailable() const override { return initialized_; }

private:
    bool initialized_ = false;

    std::shared_ptr<IUIAElement> findElementRecursive(AXUIElementRef parent, const UIASelector& selector) {
        if (!parent) return nullptr;

        auto current = std::make_shared<UIAElement>(parent);
        if (selector.matches(current->getInfo())) {
            return current;
        }

        // 递归查找子元素
        CFArrayRef children = nullptr;
        if (AXUIElementCopyAttributeValues(parent, kAXChildrenAttribute, 0, 100, &children) != kAXErrorSuccess || !children) {
            return nullptr;
        }

        CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count; ++i) {
            AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            if (!child) continue;

            CFRetain(child);
            auto result = findElementRecursive(child, selector);
            CFRelease(child);

            if (result) {
                CFRelease(children);
                return result;
            }
        }

        CFRelease(children);
        return nullptr;
    }
};

} // namespace wingman

std::unique_ptr<wingman::IUIAManager> createUIAManager() {
    return std::make_unique<wingman::UIAManager>();
}

#endif // __APPLE__
