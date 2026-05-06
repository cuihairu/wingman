#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>

namespace wingman {

// 前向声明
class UIAutomationElement;

// UI 元素信息
struct UIElementInfo {
    std::string name;
    std::string className;
    std::string automationId;
    std::string controlType;
    Rect bounds;
    bool isEnabled;
    bool isVisible;
    int handle;

    UIElementInfo() : isEnabled(false), isVisible(false), handle(0) {}
};

// 控制类型枚举
enum class UIControlType {
    Unknown = 0,
    Button = 50000,
    Calendar = 50001,
    CheckBox = 50002,
    ComboBox = 50003,
    Edit = 50004,
    Hyperlink = 50005,
    Image = 50006,
    ListItem = 50007,
    List = 50008,
    Menu = 50009,
    MenuBar = 50010,
    MenuItem = 50011,
    ProgressBar = 50012,
    RadioButton = 50013,
    ScrollBar = 50014,
    Slider = 50015,
    Spinner = 50016,
    StatusBar = 50017,
    Tab = 50018,
    TabItem = 50019,
    Text = 50020,
    ToolBar = 50021,
    ToolTip = 50022,
    Tree = 50023,
    TreeItem = 50024,
    Custom = 50025,
    Group = 50026,
    Pane = 50027,
    Window = 50028,
    DataGrid = 50029,
    DataItem = 50030,
};

// UI Automation 查找条件
struct UIACondition {
    std::string name;
    std::string className;
    std::string automationId;
    UIControlType controlType = UIControlType::Unknown;
    bool enabled = true;
    bool visible = true;

    UIACondition() = default;

    // 设置属性条件
    UIACondition& withName(const std::string& n) { name = n; return *this; }
    UIACondition& withClassName(const std::string& n) { className = n; return *this; }
    UIACondition& withAutomationId(const std::string& n) { automationId = n; return *this; }
    UIACondition& withControlType(UIControlType t) { controlType = t; return *this; }
    UIACondition& withEnabled(bool e = true) { enabled = e; return *this; }
    UIACondition& withVisible(bool v = true) { visible = v; return *this; }
};

// UI Automation 元素
class UIAutomationElement {
public:
    UIAutomationElement();
    ~UIAutomationElement();

    // 查找子元素
    std::shared_ptr<UIAutomationElement> findFirst(const UIACondition& condition);
    std::vector<std::shared_ptr<UIAutomationElement>> findAll(const UIACondition& condition);
    std::vector<std::shared_ptr<UIAutomationElement>> findAllDescendants(const UIACondition& condition);

    // 获取信息
    UIElementInfo getInfo() const;

    // 操作
    bool click();
    bool rightClick();
    bool doubleClick();
    bool focus();

    // 文本框操作
    std::string getValue() const;
    bool setValue(const std::string& value);

    // 选择操作
    std::string getSelection() const;
    bool selectItem(const std::string& item);
    std::vector<std::string> getSelectionItems() const;

    // 展开/折叠
    bool expand();
    bool collapse();
    bool isExpanded() const;

    // 检查状态
    bool isValid() const;
    bool isEnabled() const;
    bool isVisible() const;

    // 获取属性
    std::string getName() const;
    std::string getClassName() const;
    std::string getAutomationId() const;
    Rect getBounds() const;

    // 父元素和子元素
    std::shared_ptr<UIAutomationElement> getParent();
    std::vector<std::shared_ptr<UIAutomationElement>> getChildren();

private:
    friend class UIAutomation;
    struct Impl;
    std::unique_ptr<Impl> impl;

    UIAutomationElement(Impl* p);
};

// UI Automation 主类
class UIAutomation {
public:
    UIAutomation();
    ~UIAutomation();

    // 初始化（首次使用时自动调用）
    bool initialize();
    void cleanup();

    // 从窗口句柄获取根元素
    std::shared_ptr<UIAutomationElement> fromWindow(HWND hwnd);
    std::shared_ptr<UIAutomationElement> fromForegroundWindow();

    // 从鼠标位置获取元素
    std::shared_ptr<UIAutomationElement> fromPoint(int x, int y);

    // 全局查找
    std::shared_ptr<UIAutomationElement> find(const UIACondition& condition);
    std::vector<std::shared_ptr<UIAutomationElement>> findAll(const UIACondition& condition);

    // 便捷查找方法
    std::shared_ptr<UIAutomationElement> findByName(const std::string& name);
    std::shared_ptr<UIAutomationElement> findById(const std::string& automationId);
    std::shared_ptr<UIAutomationElement> findButton(const std::string& name);
    std::shared_ptr<UIAutomationElement> findEdit(const std::string& name);
    std::shared_ptr<UIAutomationElement> findText(const std::string& name);

    // 等待元素出现
    std::shared_ptr<UIAutomationElement> waitFor(const UIACondition& condition, int timeoutMs = 5000);
    std::shared_ptr<UIAutomationElement> waitForName(const std::string& name, int timeoutMs = 5000);

    // 事件监听（高级功能）
    using PropertyChangedCallback = std::function<void(const std::string& propertyName, const std::string& value)>;
    using StructureChangedCallback = std::function<void()>;

    bool registerPropertyChangedHandler(const UIACondition& condition, PropertyChangedCallback callback);
    bool registerStructureChangedHandler(const UIACondition& condition, StructureChangedCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    static UIAutomation* instance_;
};

// 全局访问
UIAutomation& uia();

} // namespace wingman
