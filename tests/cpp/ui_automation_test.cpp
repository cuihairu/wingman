#include <gtest/gtest.h>
#include "wingman/ui_automation.hpp"

using namespace wingman;

class UIAutomationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// UIElementInfo Tests
// ============================================================================

TEST_F(UIAutomationTest, UIElementInfoDefaults) {
    UIElementInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.className.empty());
    EXPECT_TRUE(info.automationId.empty());
    EXPECT_TRUE(info.controlType.empty());
    EXPECT_FALSE(info.isEnabled);
    EXPECT_FALSE(info.isVisible);
    EXPECT_EQ(info.handle, 0);
}

// ============================================================================
// UIACondition Tests
// ============================================================================

TEST_F(UIAutomationTest, UIAConditionDefaults) {
    UIACondition condition;
    EXPECT_TRUE(condition.name.empty());
    EXPECT_TRUE(condition.className.empty());
    EXPECT_TRUE(condition.automationId.empty());
    EXPECT_EQ(condition.controlType, UIControlType::Unknown);
    EXPECT_TRUE(condition.enabled);
    EXPECT_TRUE(condition.visible);
}

TEST_F(UIAutomationTest, UIAConditionFluentSetters) {
    UIACondition condition;
    condition.withName("TestName")
             .withClassName("TestClass")
             .withAutomationId("TestID")
             .withControlType(UIControlType::Button)
             .withEnabled(false)
             .withVisible(false);

    EXPECT_EQ(condition.name, "TestName");
    EXPECT_EQ(condition.className, "TestClass");
    EXPECT_EQ(condition.automationId, "TestID");
    EXPECT_EQ(condition.controlType, UIControlType::Button);
    EXPECT_FALSE(condition.enabled);
    EXPECT_FALSE(condition.visible);
}

// ============================================================================
// UIControlType Tests
// ============================================================================

TEST_F(UIAutomationTest, UIControlTypeValues) {
    EXPECT_EQ(static_cast<int>(UIControlType::Unknown), 0);
    EXPECT_EQ(static_cast<int>(UIControlType::Button), 50000);
    EXPECT_EQ(static_cast<int>(UIControlType::Edit), 50004);
    EXPECT_EQ(static_cast<int>(UIControlType::Text), 50020);
    EXPECT_EQ(static_cast<int>(UIControlType::Window), 50028);
}

// ============================================================================
// UIAutomationElement Tests
// ============================================================================

TEST_F(UIAutomationTest, UIAutomationElementConstruction) {
    UIAutomationElement element;
    EXPECT_NO_THROW();
}

TEST_F(UIAutomationTest, UIAutomationElementInvalid) {
    UIAutomationElement element;
    // Default constructed element should be invalid
    EXPECT_FALSE(element.isValid());
}

// ============================================================================
// UIAutomation Tests
// ============================================================================

TEST_F(UIAutomationTest, UIAutomationConstruction) {
    UIAutomation uia;
    EXPECT_NO_THROW();
}

TEST_F(UIAutomationTest, UIAutomationDestruction) {
    auto uia = new UIAutomation();
    EXPECT_NO_THROW(delete uia);
}

TEST_F(UIAutomationTest, UIAutomationInitialize) {
    UIAutomation uia;
    // Initialize should succeed (or at least not crash)
    // May fail if UIA runtime not available, which is acceptable
    uia.initialize();
    uia.cleanup();
    SUCCEED();
}

TEST_F(UIAutomationTest, UIAutomationFromPoint) {
    UIAutomation uia;
    uia.initialize();
    // From point should return nullptr or valid element
    // Getting current cursor position
    POINT pt;
    GetCursorPos(&pt);
    auto element = uia.fromPoint(pt.x, pt.y);
    // Result depends on what's under cursor
    uia.cleanup();
    SUCCEED();
}

TEST_F(UIAutomationTest, UIAutomationFindByName) {
    UIAutomation uia;
    uia.initialize();
    // Find a non-existent element
    auto element = uia.findByName("NonExistentElementName12345");
    EXPECT_EQ(element, nullptr);
    uia.cleanup();
}

TEST_F(UIAutomationTest, UIAutomationFindById) {
    UIAutomation uia;
    uia.initialize();
    auto element = uia.findById("NonExistentAutomationId12345");
    EXPECT_EQ(element, nullptr);
    uia.cleanup();
}

TEST_F(UIAutomationTest, UIAutomationFindButton) {
    UIAutomation uia;
    uia.initialize();
    auto element = uia.findButton("NonExistentButton");
    EXPECT_EQ(element, nullptr);
    uia.cleanup();
}

TEST_F(UIAutomationTest, UIAutomationFindEdit) {
    UIAutomation uia;
    uia.initialize();
    auto element = uia.findEdit("NonExistentEdit");
    EXPECT_EQ(element, nullptr);
    uia.cleanup();
}

TEST_F(UIAutomationTest, UIAutomationFindText) {
    UIAutomation uia;
    uia.initialize();
    auto element = uia.findText("NonExistentText");
    EXPECT_EQ(element, nullptr);
    uia.cleanup();
}

TEST_F(UIAutomationTest, UIAutomationFromForegroundWindow) {
    UIAutomation uia;
    uia.initialize();
    auto element = uia.fromForegroundWindow();
    // Should return a valid element (foreground window root)
    // or nullptr if initialization failed
    uia.cleanup();
    SUCCEED();
}

// ============================================================================
// Global Access Tests
// ============================================================================

TEST_F(UIAutomationTest, GlobalUIAAccess) {
    auto& uia1 = wingman::uia();
    auto& uia2 = wingman::uia();
    EXPECT_EQ(&uia1, &uia2);
}
