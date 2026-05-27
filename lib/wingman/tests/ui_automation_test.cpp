#include <gtest/gtest.h>
#include "wingman/ui_automation.hpp"

using namespace wingman;

// ========== UIARole 枚举 ==========

TEST(UIARoleTest, EnumValues) {
    EXPECT_NO_THROW(UIARole r = UIARole::Unknown);
    EXPECT_NO_THROW(UIARole r = UIARole::Window);
    EXPECT_NO_THROW(UIARole r = UIARole::Button);
    EXPECT_NO_THROW(UIARole r = UIARole::TextBox);
    EXPECT_NO_THROW(UIARole r = UIARole::CheckBox);
    EXPECT_NO_THROW(UIARole r = UIARole::RadioButton);
    EXPECT_NO_THROW(UIARole r = UIARole::ComboBox);
    EXPECT_NO_THROW(UIARole r = UIARole::ListBox);
    EXPECT_NO_THROW(UIARole r = UIARole::ListItem);
    EXPECT_NO_THROW(UIARole r = UIARole::Menu);
    EXPECT_NO_THROW(UIARole r = UIARole::MenuItem);
    EXPECT_NO_THROW(UIARole r = UIARole::Table);
    EXPECT_NO_THROW(UIARole r = UIARole::Tree);
    EXPECT_NO_THROW(UIARole r = UIARole::TreeItem);
}

// ========== UIAState 位操作 ==========

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

// ========== UIAutomation 构造 ==========

TEST(UIAutomationTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(UIAutomation uia);
}

TEST(UIAutomationTest, CleanupWithoutInitDoesNotCrash) {
    UIAutomation uia;
    EXPECT_NO_THROW(uia.cleanup());
}
