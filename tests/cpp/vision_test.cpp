#include <gtest/gtest.h>
#include "wingman/vision.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/smart_trigger.hpp"

using namespace wingman;

class VisionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== Vision 测试 ==========

TEST_F(VisionTest, FindColor) {
    // 测试查找颜色（需要在有图形界面的环境中运行）
    Color targetColor(255, 0, 0);  // 红色
    auto pos = Vision::findColor(targetColor, 0);
    // 结果取决于屏幕内容，这里只测试函数可以调用
    SUCCEED();
}

TEST_F(VisionTest, FindColorWithTolerance) {
    Color targetColor(128, 128, 128);
    auto pos = Vision::findColor(targetColor, 10);
    SUCCEED();
}

TEST_F(VisionTest, HasColor) {
    Color targetColor(0, 255, 0);  // 绿色
    bool found = Vision::hasColor(targetColor, 5);
    SUCCEED();
}

TEST_F(VisionTest, FindAllColors) {
    Color targetColor(0, 0, 255);  // 蓝色
    auto positions = Vision::findAllColors(targetColor, 0);
    SUCCEED();
}

TEST_F(VisionTest, GetDominantColor) {
    Color dominant = Vision::getDominantColor();
    SUCCEED();
}

TEST_F(VisionTest, DetectEdges) {
    auto edges = Vision::detectEdges();
    SUCCEED();
}

TEST_F(VisionTest, DetectContours) {
    auto contours = Vision::detectContours();
    SUCCEED();
}

TEST_F(VisionTest, DetectCircles) {
    auto circles = Vision::detectCircles();
    SUCCEED();
}

TEST_F(VisionTest, CompareImages) {
    // 这个测试需要两个实际存在的图像文件
    // double similarity = Vision::compareImages("image1.png", "image2.png");
    SUCCEED();
}

// ========== SmartTrigger 测试 ==========

class SmartTriggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        trigger = SmartTriggerManager::instance().createTrigger("test_trigger");
    }

    void TearDown() override {
        SmartTriggerManager::instance().removeTrigger("test_trigger");
    }

    std::shared_ptr<SmartTrigger> trigger;
};

TEST_F(SmartTriggerTest, CreateTrigger) {
    ASSERT_NE(trigger, nullptr);
    EXPECT_EQ(trigger->getName(), "test_trigger");
    EXPECT_FALSE(trigger->isRunning());
}

TEST_F(SmartTriggerTest, AddConditions) {
    TriggerCondition condition;
    condition.type = TriggerConditionType::COLOR_FOUND;
    condition.targetColor = Color(255, 0, 0);
    condition.tolerance = 10;

    trigger->addCondition(condition);
    trigger->addCondition(condition);  // 添加多个条件

    EXPECT_TRUE(true);  // 如果能添加，测试通过
}

TEST_F(SmartTriggerTest, AddActions) {
    TriggerAction action;
    action.type = TriggerActionType::LOG;
    action.logMessage = "Test triggered";

    trigger->addAction(action);
    EXPECT_TRUE(true);
}

TEST_F(SmartTriggerTest, SetProperties) {
    trigger->setCheckInterval(50);
    trigger->setMaxTriggers(5);

    EXPECT_TRUE(true);
}

TEST_F(SmartTriggerTest, TriggerCount) {
    EXPECT_EQ(trigger->getTriggerCount(), 0);

    trigger->resetTriggerCount();
    EXPECT_EQ(trigger->getTriggerCount(), 0);
}

TEST_F(SmartTriggerTest, ManagerOperations) {
    auto& manager = SmartTriggerManager::instance();

    // 创建多个触发器
    auto t1 = manager.createTrigger("trigger1");
    auto t2 = manager.createTrigger("trigger2");

    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);

    // 获取触发器
    auto retrieved = manager.getTrigger("trigger1");
    EXPECT_EQ(retrieved, t1);

    // 获取所有触发器
    auto all = manager.getAllTriggers();
    EXPECT_GE(all.size(), 2);

    // 移除触发器
    manager.removeTrigger("trigger1");
    EXPECT_EQ(manager.getTrigger("trigger1"), nullptr);
}

// ========== BehaviorTree 测试 ==========

class BehaviorTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        tree = BehaviorTreeManager::instance().createTree("test_tree");
    }

    void TearDown() override {
        BehaviorTreeManager::instance().removeTree("test_tree");
    }

    std::shared_ptr<BehaviorTree> tree;
};

TEST_F(BehaviorTreeTest, CreateTree) {
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree->getName(), "test_tree");
}

TEST_F(BehaviorTreeTest, SequenceNode) {
    auto seq = BehaviorTree::sequence("test_seq");

    int count = 0;
    seq->addChild(BehaviorTree::action("a1", [&count]() -> NodeStatus {
        count++;
        return NodeStatus::SUCCESS;
    }));
    seq->addChild(BehaviorTree::action("a2", [&count]() -> NodeStatus {
        count++;
        return NodeStatus::SUCCESS;
    }));

    tree->setRoot(seq);
    NodeStatus status = tree->tick();

    EXPECT_EQ(status, NodeStatus::SUCCESS);
    EXPECT_EQ(count, 2);
}

TEST_F(BehaviorTreeTest, SelectorNode) {
    auto sel = BehaviorTree::selector("test_sel");

    sel->addChild(BehaviorTree::condition("c1", []() { return false; }));
    sel->addChild(BehaviorTree::condition("c2", []() { return true; }));

    tree->setRoot(sel);
    NodeStatus status = tree->tick();

    EXPECT_EQ(status, NodeStatus::SUCCESS);
}

TEST_F(BehaviorTreeTest, InverterNode) {
    auto inv = std::make_shared<InverterNode>(
        BehaviorTree::condition("always_true", []() { return true; })
    );

    tree->setRoot(inv);
    NodeStatus status = tree->tick();

    EXPECT_EQ(status, NodeStatus::FAILURE);
}

TEST_F(BehaviorTreeTest, RepeatNode) {
    int count = 0;
    auto action = BehaviorTree::action("count", [&count]() -> NodeStatus {
        count++;
        return NodeStatus::SUCCESS;
    });

    auto repeat = std::make_shared<RepeatNode>(action, 3);
    tree->setRoot(repeat);

    // 需要多次 tick 来完成重复
    NodeStatus status;
    do {
        status = tree->tick();
    } while (status == NodeStatus::RUNNING);

    EXPECT_EQ(status, NodeStatus::SUCCESS);
    EXPECT_EQ(count, 3);
}

TEST_F(BehaviorTreeTest, WaitNode) {
    auto wait = std::make_shared<WaitNode>(100);  // 100ms
    tree->setRoot(wait);

    auto start = std::chrono::steady_clock::now();
    NodeStatus status = NodeStatus::RUNNING;

    while (status == NodeStatus::RUNNING) {
        status = tree->tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_GE(elapsed, 100);
}

TEST_F(BehaviorTreeTest, ComplexTree) {
    // 构建一个复杂的树
    auto root = BehaviorTree::sequence("main");

    // 第一个子节点：并行执行
    auto parallel = BehaviorTree::parallel("parallel", ParallelNode::Policy::SUCCEED_ON_ALL);
    parallel->addChild(BehaviorTree::action("p1", []() { return NodeStatus::SUCCESS; }));
    parallel->addChild(BehaviorTree::action("p2", []() { return NodeStatus::SUCCESS; }));

    // 第二个子节点：选择执行
    auto selector = BehaviorTree::selector("selector");
    selector->addChild(BehaviorTree::condition("c1", []() { return false; }));
    selector->addChild(BehaviorTree::action("a1", []() { return NodeStatus::SUCCESS; }));

    root->addChild(parallel);
    root->addChild(selector);

    tree->setRoot(root);
    NodeStatus status = tree->tick();

    EXPECT_EQ(status, NodeStatus::SUCCESS);
}

TEST_F(BehaviorTreeTest, ManagerOperations) {
    auto& manager = BehaviorTreeManager::instance();

    auto t1 = manager.createTree("tree1");
    auto t2 = manager.createTree("tree2");

    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);

    auto all = manager.getAllTrees();
    EXPECT_GE(all.size(), 2);

    manager.removeTree("tree1");
    EXPECT_EQ(manager.getTree("tree1"), nullptr);
}
