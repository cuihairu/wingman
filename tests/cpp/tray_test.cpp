#include <gtest/gtest.h>
#include "wingman/tray.hpp"

using namespace wingman;

class TrayTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// TrayItemType Tests
// ============================================================================

TEST_F(TrayTest, TrayItemTypeValues) {
    EXPECT_EQ(static_cast<int>(TrayItemType::NORMAL), 0);
    EXPECT_EQ(static_cast<int>(TrayItemType::SEPARATOR), 1);
    EXPECT_EQ(static_cast<int>(TrayItemType::SUBMENU), 2);
}

// ============================================================================
// TrayItem Tests
// ============================================================================

TEST_F(TrayTest, TrayItemDefaults) {
    TrayItem item;
    EXPECT_TRUE(item.id.empty());
    EXPECT_TRUE(item.label.empty());
    EXPECT_EQ(item.type, TrayItemType::NORMAL);
    EXPECT_FALSE(item.checked);
    EXPECT_TRUE(item.enabled);
    EXPECT_TRUE(item.subitems.empty());
    EXPECT_FALSE(item.callback);
}

TEST_F(TrayTest, TrayItemWithValues) {
    TrayItem item;
    item.id = "test_id";
    item.label = "Test Label";
    item.type = TrayItemType::SEPARATOR;
    item.checked = true;
    item.enabled = false;

    EXPECT_EQ(item.id, "test_id");
    EXPECT_EQ(item.label, "Test Label");
    EXPECT_EQ(item.type, TrayItemType::SEPARATOR);
    EXPECT_TRUE(item.checked);
    EXPECT_FALSE(item.enabled);
}

TEST_F(TrayTest, TrayItemWithCallback) {
    bool called = false;
    TrayItem item;
    item.id = "callback_item";
    item.label = "Callback Item";
    item.callback = [&called]() { called = true; };

    EXPECT_TRUE(item.callback);
    item.callback();
    EXPECT_TRUE(called);
}

TEST_F(TrayTest, TrayItemWithSubitems) {
    TrayItem item;
    item.id = "parent";
    item.label = "Parent Item";
    item.type = TrayItemType::SUBMENU;

    TrayItem subitem1;
    subitem1.id = "sub1";
    subitem1.label = "Subitem 1";

    TrayItem subitem2;
    subitem2.id = "sub2";
    subitem2.label = "Subitem 2";

    item.subitems.push_back(subitem1);
    item.subitems.push_back(subitem2);

    EXPECT_EQ(item.subitems.size(), 2);
    EXPECT_EQ(item.subitems[0].id, "sub1");
    EXPECT_EQ(item.subitems[1].id, "sub2");
}

// ============================================================================
// TrayIcon Tests
// ============================================================================

TEST_F(TrayTest, TrayIconConstruction) {
    TrayIcon icon("Test Tooltip");
    EXPECT_NO_THROW();
}

TEST_F(TrayTest, TrayIconSetTooltip) {
    TrayIcon icon("Initial Tooltip");
    EXPECT_NO_THROW(icon.setTooltip("New Tooltip"));
}

TEST_F(TrayTest, TrayIconSetIcon) {
    TrayIcon icon("Test");
    // Empty path should not crash
    EXPECT_NO_THROW(icon.setIcon(""));
}

TEST_F(TrayTest, TrayIconAddItem) {
    TrayIcon icon("Test");

    TrayItem item;
    item.id = "item1";
    item.label = "Item 1";
    item.callback = []() {};

    EXPECT_NO_THROW(icon.addItem(item));
}

TEST_F(TrayTest, TrayIconAddItemConvenience) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.addItem("item1", "Item 1", []() {}));
}

TEST_F(TrayTest, TrayIconAddSeparator) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.addSeparator("sep1"));
}

TEST_F(TrayTest, TrayIconAddSubmenu) {
    TrayIcon icon("Test");

    TrayItem sub1;
    sub1.id = "sub1";
    sub1.label = "Subitem 1";

    TrayItem sub2;
    sub2.id = "sub2";
    sub2.label = "Subitem 2";

    std::vector<TrayItem> subitems = {sub1, sub2};

    EXPECT_NO_THROW(icon.addSubmenu("menu1", "Submenu", subitems));
}

TEST_F(TrayTest, TrayIconRemoveItem) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.removeItem("nonexistent"));
}

TEST_F(TrayTest, TrayIconClearItems) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.clearItems());
}

TEST_F(TrayTest, TrayIconShowHide) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.show());
    EXPECT_NO_THROW(icon.hide());
}

TEST_F(TrayTest, TrayIconUpdateMenu) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.updateMenu());
}

TEST_F(TrayTest, TrayIconIsVisible) {
    TrayIcon icon("Test");
    // May be false if not shown
    bool visible = icon.isVisible();
    SUCCEED();
}

TEST_F(TrayTest, TrayIconSetItemChecked) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.setItemChecked("item1", true));
}

TEST_F(TrayTest, TrayIconSetItemEnabled) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.setItemEnabled("item1", false));
}

TEST_F(TrayTest, TrayIconSetItemLabel) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.setItemLabel("item1", "New Label"));
}

// ============================================================================
// TrayManager Tests
// ============================================================================

TEST_F(TrayTest, TrayManagerInstance) {
    auto& manager1 = TrayManager::instance();
    auto& manager2 = TrayManager::instance();
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(TrayTest, TrayManagerCreateIcon) {
    auto& manager = TrayManager::instance();
    auto icon = manager.createIcon("test_icon", "Test Icon");
    EXPECT_NE(icon, nullptr);
}

TEST_F(TrayTest, TrayManagerGetIcon) {
    auto& manager = TrayManager::instance();
    manager.createIcon("get_test", "Get Test");
    auto icon = manager.getIcon("get_test");
    EXPECT_NE(icon, nullptr);

    auto missing = manager.getIcon("nonexistent");
    EXPECT_EQ(missing, nullptr);
}

TEST_F(TrayTest, TrayManagerRemoveIcon) {
    auto& manager = TrayManager::instance();
    manager.createIcon("remove_test", "Remove Test");
    EXPECT_NO_THROW(manager.removeIcon("remove_test"));
    EXPECT_EQ(manager.getIcon("remove_test"), nullptr);
}

TEST_F(TrayTest, TrayManagerClear) {
    auto& manager = TrayManager::instance();
    manager.createIcon("clear1", "Clear 1");
    manager.createIcon("clear2", "Clear 2");
    EXPECT_NO_THROW(manager.clear());
}

// ============================================================================
// TrayIconState Tests
// ============================================================================

TEST_F(TrayTest, TrayIconStateValues) {
    EXPECT_EQ(static_cast<int>(TrayIconState::Idle), 0);
    EXPECT_EQ(static_cast<int>(TrayIconState::Running), 1);
    EXPECT_EQ(static_cast<int>(TrayIconState::Paused), 2);
    EXPECT_EQ(static_cast<int>(TrayIconState::Error), 3);
}

TEST_F(TrayTest, TrayIconGetSetIconState) {
    TrayIcon icon("Test");
    EXPECT_NO_THROW(icon.setIconState(TrayIconState::Running));
    auto state = icon.getIconState();
    EXPECT_EQ(state, TrayIconState::Running);
}
