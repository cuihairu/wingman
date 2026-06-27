#pragma once

#include "wingman/screen.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace wingman {

/**
 * @brief Element role
 */
enum class UIARole : uint16_t {
    Unknown,
    Window,
    Button,
    TextBox,
    CheckBox,
    RadioButton,
    ComboBox,
    ListBox,
    ListItem,
    Menu,
    MenuItem,
    Table,
    Tree,
    TreeItem,
};

/**
 * @brief Element state
 */
enum class UIAState : uint32_t {
    None = 0,
    Enabled = 1 << 0,
    Visible = 1 << 1,
    Focusable = 1 << 2,
    Focused = 1 << 3,
    Checkable = 1 << 4,
    Checked = 1 << 5,
    Selectable = 1 << 6,
    Selected = 1 << 7,
};

inline UIAState operator|(UIAState a, UIAState b) {
    return static_cast<UIAState>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline UIAState operator&(UIAState a, UIAState b) {
    return static_cast<UIAState>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasState(UIAState flags, UIAState state) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(state)) == static_cast<uint32_t>(state);
}

/**
 * @brief UI element information
 */
struct UIAElementInfo {
    std::string name;
    std::string id;
    std::string className;
    UIARole role = UIARole::Unknown;
    Rect bounds;
    UIAState state = UIAState::None;
    std::string text;
    bool isEnabled = true;
    bool isVisible = true;
    bool hasFocus = false;
};

/**
 * @brief Element selector
 */
struct UIASelector {
    std::string name;
    std::string id;
    std::string className;
    UIARole role = UIARole::Unknown;
    std::string text;

    UIASelector& withName(const std::string& value) { name = value; return *this; }
    UIASelector& withId(const std::string& value) { id = value; return *this; }
    UIASelector& withClassName(const std::string& value) { className = value; return *this; }
    UIASelector& withRole(UIARole value) { role = value; return *this; }
    UIASelector& withText(const std::string& value) { text = value; return *this; }

    bool matches(const UIAElementInfo& info) const {
        if (!name.empty() && info.name.find(name) == std::string::npos) return false;
        if (!id.empty() && info.id != id) return false;
        if (!className.empty() && info.className != className) return false;
        if (role != UIARole::Unknown && info.role != role) return false;
        if (!text.empty() && info.text.find(text) == std::string::npos) return false;
        return true;
    }
};

/**
 * @brief UI Automation element interface
 */
class IUIAElement {
public:
    virtual ~IUIAElement() = default;

    virtual std::string getName() const = 0;
    virtual std::string getId() const = 0;
    virtual std::string getClassName() const = 0;
    virtual UIARole getRole() const = 0;
    virtual Rect getBounds() const = 0;
    virtual bool isVisible() const = 0;
    virtual bool isEnabled() const = 0;

    virtual std::string getText() const = 0;
    virtual bool setText(const std::string& text) = 0;

    virtual bool click() = 0;
    virtual bool setFocus() = 0;
    virtual bool hasFocus() const = 0;

    virtual bool isChecked() const = 0;
    virtual bool setChecked(bool checked) = 0;

    virtual bool isValid() const = 0;
    virtual UIAElementInfo getInfo() const = 0;

    // Plan 6: 新增接口
    virtual std::vector<std::shared_ptr<IUIAElement>> getChildren() = 0;
    virtual bool expand() = 0;
    virtual bool collapse() = 0;
    virtual bool isExpanded() const = 0;
    virtual bool doubleClick() = 0;
};

/**
 * @brief UI Automation manager interface
 */
class IUIAManager {
public:
    virtual ~IUIAManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual std::shared_ptr<IUIAElement> getFocusedElement() = 0;
    virtual std::shared_ptr<IUIAElement> getElementFromPoint(const Point& point) = 0;
    virtual std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) = 0;

    // Plan 6: 新增接口
    virtual std::shared_ptr<IUIAElement> getElementFromWindow(uint64_t hwnd) = 0;
    virtual std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role) = 0;
    virtual std::shared_ptr<IUIAElement> waitForElement(const UIASelector& selector, int timeoutMs) = 0;

    virtual std::string getBackendName() const = 0;
    virtual bool isAvailable() const = 0;
};

/**
 * @brief UI Automation main class (backward compatible)
 */
class UIAutomation {
public:
    UIAutomation();
    ~UIAutomation();

    bool initialize();
    void cleanup();

    // Find methods
    std::shared_ptr<IUIAElement> find(const UIASelector& selector);
    std::shared_ptr<IUIAElement> findByName(const std::string& name);
    std::shared_ptr<IUIAElement> findById(const std::string& id);

    std::shared_ptr<IUIAElement> getFocusedElement();
    std::shared_ptr<IUIAElement> getElementFromPoint(int x, int y);

    // Plan 6: 新增门面方法
    std::shared_ptr<IUIAElement> fromWindow(uint64_t hwnd);
    std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role);
    std::shared_ptr<IUIAElement> waitForName(const std::string& name, int timeoutMs);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Global access
UIAutomation& uia();

} // namespace wingman
