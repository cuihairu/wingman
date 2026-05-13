# UI 自动化抽象层设计

> **版本**: v1.0
> **日期**: 2026-05-14
> **状态**: 设计草案

---

## 1. 概述

### 1.1 设计目标

提供跨平台的 UI 自动化接口，统一 Windows UI Automation、macOS Accessibility API 和 Linux AT-SPI。

| 目标 | 说明 |
|------|------|
| **语义统一** | 相同的操作在不同平台有一致的语义 |
| **元素定位** | 统一的元素查找和定位方式 |
| **属性访问** | 统一的属性名称和类型 |
| **事件监听** | 统一的事件模型 |
| **易于扩展** | 支持自定义选择器和操作 |

### 1.2 平台对应关系

```
┌─────────────────────────────────────────────────────────────────┐
│                     平台 API 对应关系                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  抽象概念              │  Windows           │  macOS           │
│                       │  (UI Automation)   │  (Accessibility)  │
│  ─────────────────────┼────────────────────┼──────────────────│
│  元素 (Element)        │  IUIAutomation     │  AXUIElement      │
│  树 (Tree)             │  TreeWalker        │  AXUIElement      │
│  模式 (Pattern)        │  Control Pattern   │  AXAttribute      │
│  事件 (Event)          │  Event             │  Notification    │
│                       │                    │                   │
│                       │  Linux             │                   │
│                       │  (AT-SPI)          │                   │
│  ─────────────────────┼────────────────────┴──────────────────│
│  元素 (Element)        │  AtspiAccessible   │                   │
│  树 (Tree)             │  Tree Traversal    │                   │
│  模式 (Pattern)        │  Interface         │                   │
│  事件 (Event)          │  Event             │                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 元素模型

### 2.1 元素标识

```cpp
namespace wingman::platform::uia {

/**
 * @brief 元素角色
 *
 * 定义元素的语义角色，与平台具体控件类型解耦
 */
enum class UIAElementRole : uint16_t {
    Unknown,
    // 容器类
    Window,           // 窗口/对话框
    Pane,             // 面板/分组
    Tab,              // 标签页控件
    TabPage,          // 标签页
    Menu,             // 菜单
    MenuBar,          // 菜单栏
    MenuItem,         // 菜单项
    ToolBar,          // 工具栏
    StatusBar,        // 状态栏
    Tree,             // 树控件
    TreeItem,         // 树节点
    Table,            // 表格
    Grid,             // 网格
    Group,            // 分组框
    Splitter,         // 分割器
    // 交互类
    Button,           // 按钮
    CheckBox,         // 复选框
    RadioButton,      // 单选按钮
    ToggleButton,     // 切换按钮
    ComboBox,         // 下拉框
    ListBox,          // 列表框
    ListItem,         // 列表项
    DropDownList,     // 下拉列表
    Slider,           // 滑块
    SpinButton,       // 微调按钮
    ProgressBar,      // 进度条
    ScrollBar,        // 滚动条
    // 输入类
    TextBox,          // 文本框
    RichTextBox,      // 富文本框
    PasswordBox,      // 密码框
    NumericInput,     // 数字输入
    DatePicker,       // 日期选择器
    TimePicker,       // 时间选择器
    // 显示类
    Text,             // 静态文本
    Image,            // 图片
    Icon,             // 图标
    Chart,            // 图表
    Graph,            // 图形
    StatusBar,        // 状态指示器
    Tooltip,          // 工具提示
    // 特殊类
    TitleBar,         // 标题栏
    CloseButton,      // 关闭按钮
    MinimizeButton,   // 最小化按钮
    MaximizeButton,   // 最大化按钮
    HelpButton,       // 帮助按钮
    Scroll,           // 滚动区域
    Header,           // 标题头
    HeaderItem,       // 标题项
    Footer,           // 页脚
    Separator,        // 分隔符
    // 文档类
    Document,         // 文档
    Paragraph,        // 段落
    Heading,          // 标题
    Link,             // 链接
    List,             // 列表
    Comment,          // 注释
    Code,             // 代码块
    Quote,            // 引用
};

/**
 * @brief 元素状态
 */
enum class UIAElementState : uint32_t {
    None = 0,
    Focusable = 1 << 0,
    Focused = 1 << 1,
    Clickable = 1 << 2,
    Checkable = 1 << 3,
    Checked = 1 << 4,
    Editable = 1 << 5,
    Enabled = 1 << 6,
    Visible = 1 << 7,
    Selectable = 1 << 8,
    Selected = 1 << 9,
    Expandable = 1 << 10,
    Expanded = 1 << 11,
    Collapsible = 1 << 12,
    Collapsed = 1 << 13,
    ReadOnly = 1 << 14,
    Required = 1 << 15,
    Invalid = 1 << 16,
    Busy = 1 << 17,
    Loading = 1 << 18,
    Offline = 1 << 19,
};
DEFINE_ENUM_FLAG_OPERATORS(UIAElementState);

/**
 * @brief 元素控制类型（平台特定，用于兼容）
 */
enum class UIAControlType : uint32_t {
    // Windows UIA ControlType
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
    Thumb = 50027,
    DataGrid = 50028,
    DataItem = 50029,
    Document = 50030,
    SplitButton = 50031,
    Window = 50032,
    Pane = 50033,
    Header = 50034,
    HeaderItem = 50035,
    Table = 50036,
    TitleBar = 50037,
    Separator = 50038,
};

} // namespace wingman::platform::uia
```

### 2.2 元素选择器

```cpp
/**
 * @brief 元素选择器
 *
 * 用于定位和筛选 UI 元素
 */
struct UIASelector {
    // 按属性筛选
    std::string name;                    // 名称/AutomationName
    std::string id;                      // ID/AutomationId
    std::string className;               // 类名
    std::string roleName;                // 角色名
    UIAElementRole role;                  // 角色
    UIAControlType controlType;          // 控件类型
    std::string text;                    // 文本内容
    int index = -1;                      // 同级索引
    int depth = -1;                      // 查找深度

    // 按状态筛选
    UIAElementState stateFlags = UIAElementState::None;
    bool hasState(UIAElementState state) const {
        return (stateFlags & state) == state;
    }

    // 按边界筛选
    Rect bounds;                          // 边界区域
    bool inBounds(const Point& point) const {
        return point.x >= bounds.x && point.x < bounds.x + bounds.width &&
               point.y >= bounds.y && point.y < bounds.y + bounds.height;
    }

    // 组合条件
    std::vector<UIASelector> children;    // 子元素选择器
    std::vector<UIASelector> descendants; // 后代元素选择器

    // 关系选择
    UIASelector* parent = nullptr;        // 父元素
    UIASelector* ancestor = nullptr;      // 祖先元素
    std::string siblingName;              // 兄弟元素名称

    // 构建函数
    UIASelector& withName(const std::string& n) {
        name = n;
        return *this;
    }

    UIASelector& withId(const std::string& i) {
        id = i;
        return *this;
    }

    UIASelector& withRole(UIAElementRole r) {
        role = r;
        return *this;
    }

    UIASelector& withState(UIAElementState s) {
        stateFlags = s;
        return *this;
    }

    UIASelector& atIndex(int i) {
        index = i;
        return *this;
    }

    UIASelector& withText(const std::string& t) {
        text = t;
        return *this;
    }

    // 链式选择器
    UIASelector& child(const UIASelector& sel) {
        children.push_back(sel);
        return *this;
    }

    UIASelector& descendant(const UIASelector& sel) {
        descendants.push_back(sel);
        return *this;
    }
};

/**
 * @brief 查找范围
 */
enum class UIAScope : uint8_t {
    Element,        // 当前元素
    Children,       // 直接子元素
    Descendants,    // 所有后代元素
    Subtree,        // 子树（包含当前元素）
    Parent,         // 父元素
    Ancestors,      // 所有祖先元素
};
```

---

## 3. 元素接口

### 3.1 IUIAElement 核心接口

```cpp
/**
 * @brief UI 自动化元素接口
 *
 * 提供跨平台的 UI 元素操作接口
 */
class IUIAElement {
public:
    virtual ~IUIAElement() = default;

    // ========== 基本属性 ==========

    /**
     * @brief 获取元素名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取元素 ID
     */
    virtual std::string getId() const = 0;

    /**
     * @brief 获取类名
     */
    virtual std::string getClassName() const = 0;

    /**
     * @brief 获取角色
     */
    virtual UIAElementRole getRole() const = 0;

    /**
     * @brief 获取控件类型
     */
    virtual UIAControlType getControlType() const = 0;

    /**
     * @brief 获取边界
     */
    virtual Rect getBounds() const = 0;

    /**
     * @brief 获取可见状态
     */
    virtual bool isVisible() const = 0;

    /**
     * @brief 获取启用状态
     */
    virtual bool isEnabled() const = 0;

    // ========== 文本属性 ==========

    /**
     * @brief 获取文本内容
     */
    virtual std::string getText() const = 0;

    /**
     * @brief 设置文本内容
     */
    virtual bool setText(const std::string& text) = 0;

    /**
     * @brief 获取光标位置
     */
    virtual int getCaretPosition() const = 0;

    /**
     * @brief 设置光标位置
     */
    virtual bool setCaretPosition(int position) = 0;

    /**
     * @brief 获取选中文本
     */
    virtual std::string getSelection() const = 0;

    /**
     * @brief 设置选中文本
     */
    virtual bool setSelection(int start, int end) = 0;

    // ========== 交互操作 ==========

    /**
     * @brief 点击元素
     */
    virtual bool click() = 0;

    /**
     * @brief 右键点击
     */
    virtual bool rightClick() = 0;

    /**
     * @brief 双击
     */
    virtual bool doubleClick() = 0;

    /**
     * @brief 在指定位置点击
     */
    virtual bool clickAt(int x, int y) = 0;

    /**
     * @brief 悬停
     */
    virtual bool hover() = 0;

    /**
     * @brief 按下鼠标
     */
    virtual bool mouseDown() = 0;

    /**
     * @brief 释放鼠标
     */
    virtual bool mouseUp() = 0;

    // ========== 键盘操作 ==========

    /**
     * @brief 设置焦点
     */
    virtual bool setFocus() = 0;

    /**
     * @brief 获取焦点状态
     */
    virtual bool hasFocus() const = 0;

    /**
     * @brief 输入文本
     */
    virtual bool typeText(const std::string& text) = 0;

    /**
     * @brief 按键
     */
    virtual bool typeKey(KeyCode key) = 0;

    // ========== 模式特定操作 ==========

    /**
     * @brief 获取/设置值（适用于 Slider, ProgressBar 等）
     */
    virtual std::string getValue() const = 0;
    virtual bool setValue(const std::string& value) = 0;

    /**
     * @brief 获取/设置范围值
     */
    virtual double getRangeValue() const = 0;
    virtual bool setRangeValue(double value) = 0;
    virtual double getMinimum() const = 0;
    virtual double getMaximum() const = 0;

    /**
     * @brief 获取/设置切换状态（CheckBox, ToggleButton 等）
     */
    virtual bool isChecked() const = 0;
    virtual bool setChecked(bool checked) = 0;

    /**
     * @brief 获取/设置展开状态
     */
    virtual bool isExpanded() const = 0;
    virtual bool setExpanded(bool expanded) = 0;

    /**
     * @brief 获取选项状态（RadioButton, ListItem 等）
     */
    virtual bool isSelected() const = 0;
    virtual bool setSelected(bool selected) = 0;

    // ========== 滚动操作 ==========

    /**
     * @brief 滚动到视图
     */
    virtual bool scrollIntoView() = 0;

    /**
     * @brief 滚动
     */
    virtual bool scroll(double horizontalPercent, double verticalPercent) = 0;

    // ========== 导航操作 ==========

    /**
     * @brief 获取父元素
     */
    virtual std::shared_ptr<IUIAElement> getParent() const = 0;

    /**
     * @brief 获取子元素
     */
    virtual std::vector<std::shared_ptr<IUIAElement>> getChildren() const = 0;

    /**
     * @brief 获取后代元素
     */
    virtual std::vector<std::shared_ptr<IUIAElement>> getDescendants() const = 0;

    /**
     * @brief 查找单个元素
     */
    virtual std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) = 0;

    /**
     * @brief 查找多个元素
     */
    virtual std::vector<std::shared_ptr<IUIAElement>> findElements(const UIASelector& selector) = 0;

    /**
     * @brief 等待元素出现
     */
    virtual bool waitForElement(const UIASelector& selector, int timeoutMs = 5000) = 0;

    // ========== 模式接口 ==========

    /**
     * @brief 获取支持的模式
     */
    virtual std::vector<std::string> getSupportedPatterns() const = 0;

    /**
     * @brief 检查是否支持模式
     */
    virtual bool supportsPattern(const std::string& patternName) const = 0;

    // ========== 截图和调试 ==========

    /**
     * @brief 截取元素图像
     */
    virtual std::unique_ptr<Bitmap> capture() const = 0;

    /**
     * @brief 高亮显示元素（调试用）
     */
    virtual bool highlight(int durationMs = 1000) = 0;

    /**
     * @brief 获取元素调试信息
     */
    virtual std::string getDebugInfo() const = 0;

    // ========== 生命周期 ==========

    /**
     * @brief 检查元素是否有效
     */
    virtual bool isValid() const = 0;

    /**
     * @brief 刷新元素缓存
     */
    virtual bool refresh() = 0;
};
```

---

## 4. UIA 管理器接口

### 4.1 IUIAManager

```cpp
/**
 * @brief UI 自动化管理器接口
 *
 * 提供全局 UI 自动化功能入口
 */
class IUIAManager {
public:
    virtual ~IUIAManager() = default;

    /**
     * @brief 初始化管理器
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭管理器
     */
    virtual void shutdown() = 0;

    // ========== 根元素访问 ==========

    /**
     * @brief 获取桌面元素
     */
    virtual std::shared_ptr<IUIAElement> getDesktopElement() = 0;

    /**
     * @brief 获取焦点元素
     */
    virtual std::shared_ptr<IUIAElement> getFocusedElement() = 0;

    /**
     * @brief 获取鼠标下的元素
     */
    virtual std::shared_ptr<IUIAElement> getElementFromPoint(const Point& point) = 0;

    // ========== 窗口访问 ==========

    /**
     * @brief 获取所有窗口元素
     */
    virtual std::vector<std::shared_ptr<IUIAElement>> getWindowElements() = 0;

    /**
     * @brief 按标题查找窗口元素
     */
    virtual std::shared_ptr<IUIAElement> findWindow(const std::string& title) = 0;

    // ========== 全局查找 ==========

    /**
     * @brief 全局查找元素
     */
    virtual std::shared_ptr<IUIAElement> findElement(const UIASelector& selector) = 0;

    virtual std::vector<std::shared_ptr<IUIAElement>> findElements(const UIASelector& selector) = 0;

    // ========== 事件监听 ==========

    /**
     * @brief 事件类型
     */
    enum class UIAEventType : uint32_t {
        ElementAdded,
        ElementRemoved,
        FocusChanged,
        PropertyChanged,
        Invoked,
        TextChanged,
        SelectionChanged,
        WindowOpened,
        WindowClosed,
        StructureChanged,
    };

    /**
     * @brief 事件回调
     */
    using UIAEventCallback = std::function<void(
        UIAEventType eventType,
        std::shared_ptr<IUIAElement> element,
        const std::string& eventData
    )>;

    /**
     * @brief 添加事件监听
     */
    virtual uint64_t addEventListener(
        UIAEventType eventType,
        const UIASelector& targetSelector,
        UIAEventCallback callback
    ) = 0;

    /**
     * @brief 移除事件监听
     */
    virtual bool removeEventListener(uint64_t listenerId) = 0;

    /**
     * @brief 移除所有事件监听
     */
    virtual void removeAllEventListeners() = 0;

    // ========== 辅助功能 ==========

    /**
     * @brief 获取所有顶层窗口
     */
    virtual std::vector<std::shared_ptr<IUIAElement>> getTopLevelWindows() = 0;

    /**
     * @brief 获取鼠标位置
     */
    virtual Point getMousePosition() = 0;

    // ========== 后端信息 ==========

    /**
     * @brief 获取后端名称
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief 获取后端版本
     */
    virtual std::string getBackendVersion() const = 0;
};
```

---

## 5. 使用示例

### 5.1 基本操作

```cpp
using namespace wingman::platform::uia;

auto& factory = getPlatformFactory();
auto uiaManager = factory.createUIA();

// 查找窗口
auto window = uiaManager->findWindow("Notepad");

// 查找菜单项
auto fileMenu = window->findElement(
    UIASelector().withRole(UIAElementRole::MenuBar)
                 .child(UIASelector().withName("File"))
);

// 点击菜单
fileMenu->click();

// 等待对话框出现
auto dialog = window->waitForElement(
    UIASelector().withRole(UIAElementRole::Window)
                 .child(UIASelector().withName("Save As"))
);

// 查找文本框并输入
auto textBox = dialog->findElement(
    UIASelector().withRole(UIAElementRole::TextBox).withId("FileNameBox")
);
textBox->setText("document.txt");

// 查找按钮并点击
auto saveButton = dialog->findElement(
    UIASelector().withRole(UIAElementRole::Button).withName("Save")
);
saveButton->click();
```

### 5.2 事件监听

```cpp
// 监听焦点变化
uint64_t listenerId = uiaManager->addEventListener(
    UIAEventType::FocusChanged,
    UIASelector(),  // 所有元素
    [](UIAEventType type, auto element, const auto& data) {
        spdlog::info("Focus changed to: {}", element->getName());
    }
);

// 监听窗口打开
uiaManager->addEventListener(
    UIAEventType::WindowOpened,
    UIASelector().withRole(UIAElementRole::Window),
    [](UIAEventType type, auto element, const auto& data) {
        spdlog::info("Window opened: {}", element->getName());
    }
);
```

### 5.3 表格操作

```cpp
// 查找表格
auto table = window->findElement(
    UIASelector().withRole(UIAElementRole::Table).withId("DataTable")
);

// 获取所有行
auto rows = table->findElements(
    UIASelector().withRole(UIAElementRole::ListItem)
);

// 遍历行
for (auto& row : rows) {
    auto cells = row->findElements(
        UIASelector().withRole(UIAElementRole::Text)
    );
    for (auto& cell : cells) {
        std::cout << cell->getText() << "\t";
    }
    std::cout << std::endl;
}
```

---

## 6. 平台实现映射

### 6.1 Windows UI Automation

| 抽象接口 | Windows UIA |
|----------|-------------|
| `IUIAElement` | `IUIAutomationElement` |
| `getName()` | `get_CurrentName()` |
| `getId()` | `get_CurrentAutomationId()` |
| `getRole()` | `get_CurrentControlType()` → 转换为 `UIAElementRole` |
| `getText()` | `TextPattern` / `get_CurrentName()` |
| `click()` | `InvokePattern` / `LegacyIAccessiblePattern` |
| `setValue()` | `ValuePattern` |
| `setChecked()` | `TogglePattern` / `SelectionItemPattern` |

### 6.2 macOS Accessibility

| 抽象接口 | macOS Accessibility |
|----------|-------------------|
| `IUIAElement` | `AXUIElementRef` |
| `getName()` | `AXRoleDescription` / `AXTitle` |
| `getId()` | 自定义属性 |
| `getRole()` | `AXRole` → 转换为 `UIAElementRole` |
| `getText()` | `AXValue` / `AXTitle` |
| `click()` | `AXPress` |
| `setValue()` | `AXSetValue` |
| `setChecked()` | `AXSetValue(1)` |

### 6.3 Linux AT-SPI

| 抽象接口 | AT-SPI |
|----------|--------|
| `IUIAElement` | `AtspiAccessible` |
| `getName()` | `get_name()` |
| `getId()` | `get_accessible_id()` |
| `getRole()` | `get_role()` → 转换为 `UIAElementRole` |
| `getText()` | `get_text()` / `Text` interface |
| `click()` | `get_action()` → `click` |
| `setValue()` | `set_current_value()` |
| `setChecked()` | `set_current_value(true)` |

---

## 7. 目录结构

```
lib/wingman/
├── include/wingman/platform/uia/
│   ├── iuia_element.hpp         # 元素接口
│   ├── iuia_manager.hpp         # 管理器接口
│   ├── uia_types.hpp            # 类型定义
│   ├── uia_selector.hpp         # 选择器定义
│   └── uia_event.hpp            # 事件定义
│
└── src/platform/uia/
    ├── win/
    │   └── uiautomation_manager.cpp    # Windows UIA 实现
    ├── mac/
    │   └── accessibility_manager.cpp   # macOS AX 实现
    ├── linux/
    │   └── atspi_manager.cpp           # Linux AT-SPI 实现
    └── mock/
        └── mock_uia_manager.cpp         # Mock 实现
```
