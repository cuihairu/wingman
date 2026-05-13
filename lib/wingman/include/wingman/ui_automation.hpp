#pragma once

#include "wingman/screen.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace wingman {

/**
 * @brief 元素角色
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
 * @brief 元素状态
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

inline bool hasState(UIAState flags, UIAState state) {
    return (flags & state) == state;
}

/**
 * @brief UI 元素信息
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
 * @brief 元素选择器
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
 * @brief UI 自动化元素接口
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
};

/**
 * @brief UI 自动化管理器接口
 */
class IUIAManager {
public:
    virtual ~IUIAManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual std::shared_ptr<IUIAElement> getFocusedElement() = 0;
    virtual std::shared_ptr<IUIAElement> getElementFromPoint(const Point& point) = 0;
    virtual std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) = 0;

    virtual std::string getBackendName() const = 0;
    virtual bool isAvailable() const = 0;
};

/**
 * @brief UI Automation 主类（兼容旧代码）
 */
class UIAutomation {
public:
    UIAutomation();
    ~UIAutomation();

    bool initialize();
    void cleanup();

    // 查找方法
    std::shared_ptr<IUIAElement> find(const UIASelector& selector);
    std::shared_ptr<IUIAElement> findByName(const std::string& name);
    std::shared_ptr<IUIAElement> findById(const std::string& id);

    std::shared_ptr<IUIAElement> getFocusedElement();
    std::shared_ptr<IUIAElement> getElementFromPoint(int x, int y);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// 全局访问
UIAutomation& uia();

} // namespace wingman
