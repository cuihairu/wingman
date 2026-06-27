#include <gtest/gtest.h>
#include "wingman/ui_automation.hpp"

using namespace wingman;

// ========== UIARole Enum ==========

TEST(UIARoleTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(UIARole::Unknown), 0);
    EXPECT_EQ(static_cast<int>(UIARole::Window), 1);
    EXPECT_EQ(static_cast<int>(UIARole::Button), 2);
    EXPECT_EQ(static_cast<int>(UIARole::TextBox), 3);
    EXPECT_EQ(static_cast<int>(UIARole::CheckBox), 4);
    EXPECT_EQ(static_cast<int>(UIARole::RadioButton), 5);
    EXPECT_EQ(static_cast<int>(UIARole::ComboBox), 6);
    EXPECT_EQ(static_cast<int>(UIARole::ListBox), 7);
    EXPECT_EQ(static_cast<int>(UIARole::ListItem), 8);
    EXPECT_EQ(static_cast<int>(UIARole::Menu), 9);
    EXPECT_EQ(static_cast<int>(UIARole::MenuItem), 10);
    EXPECT_EQ(static_cast<int>(UIARole::Table), 11);
    EXPECT_EQ(static_cast<int>(UIARole::Tree), 12);
    EXPECT_EQ(static_cast<int>(UIARole::TreeItem), 13);
}

// ========== UIAState Bitwise Operations ==========

TEST(UIAStateTest, BitwiseOr) {
    auto combined = UIAState::Enabled | UIAState::Visible;
    EXPECT_TRUE(hasState(combined, UIAState::Enabled));
    EXPECT_TRUE(hasState(combined, UIAState::Visible));
    EXPECT_FALSE(hasState(combined, UIAState::Focused));
}

TEST(UIAStateTest, BitwiseAnd) {
    auto combined = UIAState::Enabled | UIAState::Visible | UIAState::Focused;
    auto masked = combined & UIAState::Focused;
    EXPECT_TRUE(hasState(masked, UIAState::Focused));
}

TEST(UIAStateTest, HasState) {
    auto flags = UIAState::Enabled | UIAState::Checked;
    EXPECT_TRUE(hasState(flags, UIAState::Enabled));
    EXPECT_TRUE(hasState(flags, UIAState::Checked));
    EXPECT_FALSE(hasState(flags, UIAState::Focused));
    EXPECT_FALSE(hasState(flags, UIAState::Visible));
}

TEST(UIAStateTest, NoneState) {
    EXPECT_FALSE(hasState(UIAState::None, UIAState::Enabled));
    EXPECT_FALSE(hasState(UIAState::None, UIAState::Visible));
}

TEST(UIAStateTest, CombinedFlags) {
    auto all = UIAState::Enabled | UIAState::Visible | UIAState::Focusable |
               UIAState::Focused | UIAState::Checkable | UIAState::Checked |
               UIAState::Selectable | UIAState::Selected;
    EXPECT_TRUE(hasState(all, UIAState::Enabled));
    EXPECT_TRUE(hasState(all, UIAState::Visible));
    EXPECT_TRUE(hasState(all, UIAState::Focused));
    EXPECT_TRUE(hasState(all, UIAState::Checked));
    EXPECT_TRUE(hasState(all, UIAState::Selected));
}

// ========== UIAElementInfo ==========

TEST(UIAElementInfoTest, DefaultValues) {
    UIAElementInfo info{};
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.className.empty());
    EXPECT_EQ(info.role, UIARole::Unknown);
    EXPECT_EQ(info.state, UIAState::None);
    EXPECT_TRUE(info.text.empty());
    EXPECT_TRUE(info.isEnabled);
    EXPECT_TRUE(info.isVisible);
    EXPECT_FALSE(info.hasFocus);
}

// ========== UIASelector ==========

TEST(UIASelectorTest, FluentBuilder) {
    auto sel = UIASelector()
        .withName("btn")
        .withId("button1")
        .withClassName("Button")
        .withRole(UIARole::Button)
        .withText("Click");

    EXPECT_EQ(sel.name, "btn");
    EXPECT_EQ(sel.id, "button1");
    EXPECT_EQ(sel.className, "Button");
    EXPECT_EQ(sel.role, UIARole::Button);
    EXPECT_EQ(sel.text, "Click");
}

TEST(UIASelectorTest, MatchesByName) {
    UIASelector sel;
    sel.name = "Submit";

    UIAElementInfo info;
    info.name = "Submit Button";
    EXPECT_TRUE(sel.matches(info));

    info.name = "Cancel";
    EXPECT_FALSE(sel.matches(info));
}

TEST(UIASelectorTest, MatchesById) {
    UIASelector sel;
    sel.id = "btn1";

    UIAElementInfo info;
    info.id = "btn1";
    EXPECT_TRUE(sel.matches(info));

    info.id = "btn2";
    EXPECT_FALSE(sel.matches(info));
}

TEST(UIASelectorTest, MatchesByClassName) {
    UIASelector sel;
    sel.className = "PushButton";

    UIAElementInfo info;
    info.className = "PushButton";
    EXPECT_TRUE(sel.matches(info));

    info.className = "ToggleButton";
    EXPECT_FALSE(sel.matches(info));
}

TEST(UIASelectorTest, MatchesByRole) {
    UIASelector sel;
    sel.role = UIARole::Button;

    UIAElementInfo info;
    info.role = UIARole::Button;
    EXPECT_TRUE(sel.matches(info));

    info.role = UIARole::TextBox;
    EXPECT_FALSE(sel.matches(info));
}

TEST(UIASelectorTest, MatchesByText) {
    UIASelector sel;
    sel.text = "Hello";

    UIAElementInfo info;
    info.text = "Hello World";
    EXPECT_TRUE(sel.matches(info));

    info.text = "Goodbye";
    EXPECT_FALSE(sel.matches(info));
}

TEST(UIASelectorTest, MatchesAllCriteria) {
    UIASelector sel;
    sel.name = "btn";
    sel.role = UIARole::Button;

    UIAElementInfo info;
    info.name = "btnOK";
    info.role = UIARole::Button;
    EXPECT_TRUE(sel.matches(info));

    info.role = UIARole::TextBox;
    EXPECT_FALSE(sel.matches(info));
}

TEST(UIASelectorTest, EmptySelectorMatchesAll) {
    UIASelector sel;

    UIAElementInfo info;
    info.name = "anything";
    info.id = "any";
    EXPECT_TRUE(sel.matches(info));
}

// ========== UIAutomation Construction ==========

TEST(UIAutomationTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(UIAutomation uia);
}

TEST(UIAutomationTest, CleanupWithoutInitDoesNotCrash) {
    UIAutomation uia;
    EXPECT_NO_THROW(uia.cleanup());
}

// ========== Additional UIA Tests ==========

TEST(UIAElementInfoTest, FieldAssignment) {
    UIAElementInfo info{};
    info.name = "MyButton";
    info.id = "btn1";
    info.className = "ButtonClass";
    info.role = UIARole::Button;
    info.bounds = Rect(10, 20, 100, 40);
    info.state = UIAState::Enabled | UIAState::Visible;
    info.text = "Click Me";
    info.isEnabled = false;
    info.isVisible = false;
    info.hasFocus = true;

    EXPECT_EQ(info.name, "MyButton");
    EXPECT_EQ(info.id, "btn1");
    EXPECT_EQ(info.className, "ButtonClass");
    EXPECT_EQ(info.role, UIARole::Button);
    EXPECT_EQ(info.bounds.width, 100);
    EXPECT_TRUE(hasState(info.state, UIAState::Enabled));
    EXPECT_FALSE(info.isEnabled);
    EXPECT_FALSE(info.isVisible);
    EXPECT_TRUE(info.hasFocus);
}

TEST(UIASelectorTest, DefaultValues) {
    UIASelector sel;
    EXPECT_TRUE(sel.name.empty());
    EXPECT_TRUE(sel.id.empty());
    EXPECT_TRUE(sel.className.empty());
    EXPECT_EQ(sel.role, UIARole::Unknown);
    EXPECT_TRUE(sel.text.empty());
}

TEST(UIASelectorTest, FluentChaining) {
    auto sel = UIASelector()
        .withName("a")
        .withId("b")
        .withClassName("c")
        .withRole(UIARole::TextBox)
        .withText("d");

    EXPECT_EQ(sel.name, "a");
    EXPECT_EQ(sel.id, "b");
    EXPECT_EQ(sel.className, "c");
    EXPECT_EQ(sel.role, UIARole::TextBox);
    EXPECT_EQ(sel.text, "d");
}

TEST(UIASelectorTest, MatchesByNameSubstring) {
    UIASelector sel;
    sel.name = "Sub";

    UIAElementInfo info;
    info.name = "Submit Button";
    EXPECT_TRUE(sel.matches(info));

    info.name = "SUBMIT";
    EXPECT_FALSE(sel.matches(info)); // case sensitive
}

TEST(UIASelectorTest, MatchesByTextSubstring) {
    UIASelector sel;
    sel.text = "world";

    UIAElementInfo info;
    info.text = "Hello world!";
    EXPECT_TRUE(sel.matches(info));
}

TEST(UIAStateTest, SingleFlag) {
    EXPECT_TRUE(hasState(UIAState::Enabled, UIAState::Enabled));
    EXPECT_FALSE(hasState(UIAState::Enabled, UIAState::Visible));
}

TEST(UIAStateTest, OrAndRoundtrip) {
    auto combined = UIAState::Enabled | UIAState::Checkable | UIAState::Checked;
    auto andResult = combined & UIAState::Checked;
    EXPECT_TRUE(hasState(andResult, UIAState::Checked));
}

TEST(UIAutomationTest, FindWithoutInitReturnsNull) {
    UIAutomation uia;
    auto elem = uia.find(UIASelector().withName("nonexistent"));
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, FindByNameWithoutInitReturnsNull) {
    UIAutomation uia;
    auto elem = uia.findByName("button");
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, FindByIdWithoutInitReturnsNull) {
    UIAutomation uia;
    auto elem = uia.findById("id1");
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, GetFocusedElementWithoutInit) {
    UIAutomation uia;
    auto elem = uia.getFocusedElement();
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, GetElementFromPointWithoutInit) {
    UIAutomation uia;
    auto elem = uia.getElementFromPoint(100, 100);
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, GlobalUiaFunction) {
    EXPECT_NO_THROW(uia());
}

// ========== Plan 6: 新增接口烟雾测试 ==========

TEST(UIAutomationTest, FromWindowWithoutInitReturnsNull) {
    UIAutomation uia;
    auto elem = uia.fromWindow(0);
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, FindAllByRoleWithoutInitReturnsEmpty) {
    UIAutomation uia;
    auto elems = uia.findAllByRole(UIARole::Button);
    EXPECT_TRUE(elems.empty());
}

TEST(UIAutomationTest, WaitForNameWithoutInitReturnsNull) {
    UIAutomation uia;
    auto elem = uia.waitForName("nonexistent", 100);
    EXPECT_EQ(elem, nullptr);
}

TEST(UIAutomationTest, WaitForNameTimeoutDoesNotCrash) {
    UIAutomation uia;
    // 短超时，验证轮询循环不崩溃
    EXPECT_NO_THROW(uia.waitForName("anything", 50));
}
