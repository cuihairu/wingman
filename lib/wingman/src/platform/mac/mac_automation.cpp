#include "wingman/ui_automation.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <thread>
#include <chrono>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>

namespace wingman {

namespace {

bool copyBoolAttribute(AXUIElementRef element, CFStringRef attribute, bool defaultValue) {
    if (!element) return defaultValue;

    CFTypeRef value = nullptr;
    if (AXUIElementCopyAttributeValue(element, attribute, &value) != kAXErrorSuccess || !value) {
        return defaultValue;
    }

    bool result = defaultValue;
    CFTypeID typeId = CFGetTypeID(value);
    if (typeId == CFBooleanGetTypeID()) {
        result = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    } else if (typeId == CFNumberGetTypeID()) {
        int intValue = 0;
        if (CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType, &intValue)) {
            result = intValue != 0;
        }
    }

    CFRelease(value);
    return result;
}

} // namespace

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
            if (AXValueGetValue(position, static_cast<AXValueType>(kAXValueCGPointType), &pt) &&
                AXValueGetValue(size, static_cast<AXValueType>(kAXValueCGSizeType), &sz)) {

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
        CFTypeRef value = nullptr;
        if (AXUIElementCopyAttributeValue(element_, kAXValueAttribute, &value) == kAXErrorSuccess && value &&
            CFGetTypeID(value) == CFStringGetTypeID()) {
            CFStringRef stringValue = static_cast<CFStringRef>(value);
            std::string result = cfStringToStdString(stringValue);
            CFRelease(value);
            return result;
        }
        if (value) {
            CFRelease(value);
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
        return copyBoolAttribute(element_, kAXValueAttribute, false);
    }

    bool setChecked(bool checked) override {
        if (!element_) return false;
        CFBooleanRef cfValue = checked ? kCFBooleanTrue : kCFBooleanFalse;
        AXError err = AXUIElementSetAttributeValue(element_, kAXValueAttribute, cfValue);
        return err == kAXErrorSuccess;
    }

    bool isValid() const override {
        if (!element_) return false;
        CFBooleanRef focused = nullptr;
        AXError err = AXUIElementCopyAttributeValue(element_, kAXFocusedAttribute, (CFTypeRef*)&focused);
        if (focused) {
            CFRelease(focused);
        }
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

    // Plan 6: 新增方法实现
    std::vector<std::shared_ptr<IUIAElement>> getChildren() override {
        std::vector<std::shared_ptr<IUIAElement>> results;
        if (!element_) return results;
        CFArrayRef children = nullptr;
        if (AXUIElementCopyAttributeValues(element_, kAXChildrenAttribute, 0, 100, &children) != kAXErrorSuccess || !children) {
            return results;
        }
        CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count; ++i) {
            AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            if (child) results.push_back(std::make_shared<UIAElement>(child)); // UIAElement 构造会 CFRetain
        }
        CFRelease(children);
        return results;
    }

    bool expand() override {
        if (!element_) return false;
        return AXUIElementSetAttributeValue(element_, kAXExpandedAttribute, kCFBooleanTrue) == kAXErrorSuccess;
    }

    bool collapse() override {
        if (!element_) return false;
        return AXUIElementSetAttributeValue(element_, kAXExpandedAttribute, kCFBooleanFalse) == kAXErrorSuccess;
    }

    bool isExpanded() const override {
        return copyBoolAttribute(element_, kAXExpandedAttribute, false);
    }

    bool doubleClick() override {
        if (!element_) return false;
        Rect b = getBounds();
        if (b.width <= 0 || b.height <= 0) return false;
        CGPoint center = CGPointMake(b.x + b.width / 2.0, b.y + b.height / 2.0);
        CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, center, kCGMouseButtonLeft);
        if (move) { CGEventPost(kCGHIDEventTap, move); CFRelease(move); }
        for (int i = 1; i <= 2; ++i) {
            CGEventRef down = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseDown, center, kCGMouseButtonLeft);
            CGEventRef up = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseUp, center, kCGMouseButtonLeft);
            if (down) { CGEventSetIntegerValueField(down, kCGMouseEventClickState, i); CGEventPost(kCGHIDEventTap, down); CFRelease(down); }
            if (up) { CGEventSetIntegerValueField(up, kCGMouseEventClickState, i); CGEventPost(kCGHIDEventTap, up); CFRelease(up); }
        }
        return true;
    }

private:
    AXUIElementRef element_;

    static std::string cfStringToStdString(CFStringRef cfStr) {
        if (!cfStr) return "";
        const char* ptr = CFStringGetCStringPtr(cfStr, kCFStringEncodingUTF8);
        if (ptr) return std::string(ptr);

        CFIndex length = CFStringGetLength(cfStr);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::vector<char> buffer(maxSize);
        if (CFStringGetCString(cfStr, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
            return std::string(buffer.data());
        }
        return "";
    }

    static UIARole roleToElementRole(CFStringRef role) {
        if (!role) return UIARole::Unknown;
        if (CFEqual(role, kAXWindowRole)) return UIARole::Window;
        if (CFEqual(role, kAXButtonRole)) return UIARole::Button;
        if (CFEqual(role, kAXCheckBoxRole)) return UIARole::CheckBox;
        if (CFEqual(role, kAXRadioButtonRole)) return UIARole::RadioButton;
        if (CFEqual(role, kAXTextFieldRole)) return UIARole::TextBox;
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

        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        if (!systemWide) return nullptr;

        AXUIElementRef element = nullptr;
        AXError err = AXUIElementCopyElementAtPosition(systemWide, point.x, point.y, &element);
        CFRelease(systemWide);
        if (err != kAXErrorSuccess || !element) return nullptr;

        auto result = std::make_shared<UIAElement>(element);
        CFRelease(element);
        return result;
    }

    std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) override {
        if (!initialized_) return nullptr;

        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        if (!systemWide) return nullptr;

        AXUIElementRef focusedApp = nullptr;
        AXError err = AXUIElementCopyAttributeValue(systemWide, kAXFocusedApplicationAttribute,
                                                    reinterpret_cast<CFTypeRef*>(&focusedApp));
        CFRelease(systemWide);
        if (err != kAXErrorSuccess || !focusedApp) {
            return nullptr;
        }

        auto result = findElementRecursive(focusedApp, selector);
        CFRelease(focusedApp);
        return result;
    }

    // Plan 6: 新增方法实现
    std::shared_ptr<IUIAElement> getElementFromWindow(uint64_t hwnd) override {
        if (!initialized_) return nullptr;
        // Mac 无 HWND 概念，hwnd 参数解释为目标进程 PID（文档注明）
        pid_t pid = static_cast<pid_t>(hwnd);
        AXUIElementRef app = AXUIElementCreateApplication(pid);
        if (!app) return nullptr;
        auto result = std::make_shared<UIAElement>(app);
        CFRelease(app);
        return result;
    }

    std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role) override {
        std::vector<std::shared_ptr<IUIAElement>> results;
        if (!initialized_ || role == UIARole::Unknown) return results;
        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        if (!systemWide) return results;
        AXUIElementRef focusedApp = nullptr;
        if (AXUIElementCopyAttributeValue(systemWide, kAXFocusedApplicationAttribute,
                                          reinterpret_cast<CFTypeRef*>(&focusedApp)) == kAXErrorSuccess && focusedApp) {
            findAllRecursive(focusedApp, role, results);
            CFRelease(focusedApp);
        }
        CFRelease(systemWide);
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

        // Recursively find child elements
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

    void findAllRecursive(AXUIElementRef parent, UIARole role, std::vector<std::shared_ptr<IUIAElement>>& results) {
        if (!parent) return;
        auto current = std::make_shared<UIAElement>(parent);
        if (current->getRole() == role) results.push_back(current);
        CFArrayRef children = nullptr;
        if (AXUIElementCopyAttributeValues(parent, kAXChildrenAttribute, 0, 100, &children) != kAXErrorSuccess || !children) {
            return;
        }
        CFIndex count = CFArrayGetCount(children);
        for (CFIndex i = 0; i < count; ++i) {
            AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
            if (!child) continue;
            CFRetain(child);
            findAllRecursive(child, role, results);
            CFRelease(child);
        }
        CFRelease(children);
    }
};

} // namespace wingman

std::unique_ptr<wingman::IUIAManager> createUIAManager() {
    return std::make_unique<wingman::UIAManager>();
}

#endif // __APPLE__
