#include <gtest/gtest.h>
#include "wingman/behavior_tree.hpp"

using namespace wingman;

class BehaviorTreeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// BehaviorContext Tests
// ============================================================================

TEST_F(BehaviorTreeTest, BehaviorContextVariables) {
    BehaviorContext ctx;
    ctx.set("test_key", "test_value");
    auto value = ctx.get<std::string>("test_key", "");
    EXPECT_EQ(value, "test_value");

    auto missing = ctx.get<std::string>("missing_key", "default");
    EXPECT_EQ(missing, "default");
}

TEST_F(BehaviorTreeTest, BehaviorContextNumbers) {
    BehaviorContext ctx;
    ctx.set("pi", 3.14159);
    auto value = ctx.get<double>("pi", 0.0);
    EXPECT_NEAR(value, 3.14159, 0.00001);

    auto missing = ctx.get<double>("missing_key", 42.0);
    EXPECT_DOUBLE_EQ(missing, 42.0);
}

TEST_F(BehaviorTreeTest, BehaviorContextFlags) {
    BehaviorContext ctx;
    ctx.set("flag_true", true);
    ctx.set("flag_false", false);
    EXPECT_TRUE(ctx.get<bool>("flag_true", false));
    EXPECT_FALSE(ctx.get<bool>("flag_false", true));
    EXPECT_TRUE(ctx.get<bool>("missing", true));
}

// ============================================================================
// SequenceNode Tests
// ============================================================================

class MockSuccessNode : public BehaviorNode {
public:
    int tickCount = 0;
    NodeStatus tick() override {
        tickCount++;
        return NodeStatus::SUCCESS;
    }
    std::string getName() const override { return "MockSuccess"; }
};

class MockFailureNode : public BehaviorNode {
public:
    int tickCount = 0;
    NodeStatus tick() override {
        tickCount++;
        return NodeStatus::FAILURE;
    }
    std::string getName() const override { return "MockFailure"; }
};

class MockRunningNode : public BehaviorNode {
public:
    int tickCount = 0;
    NodeStatus tick() override {
        tickCount++;
        return NodeStatus::RUNNING;
    }
    std::string getName() const override { return "MockRunning"; }
};

TEST_F(BehaviorTreeTest, SequenceNodeAllSuccess) {
    SequenceNode sequence("TestSequence");
    auto child1 = std::make_shared<MockSuccessNode>();
    auto child2 = std::make_shared<MockSuccessNode>();
    auto child3 = std::make_shared<MockSuccessNode>();

    sequence.addChild(child1);
    sequence.addChild(child2);
    sequence.addChild(child3);

    EXPECT_EQ(sequence.tick(), NodeStatus::SUCCESS);
    EXPECT_EQ(child1->tickCount, 1);
    EXPECT_EQ(child2->tickCount, 1);
    EXPECT_EQ(child3->tickCount, 1);
}

TEST_F(BehaviorTreeTest, SequenceNodeFirstFailure) {
    SequenceNode sequence("TestSequence");
    auto child1 = std::make_shared<MockSuccessNode>();
    auto child2 = std::make_shared<MockFailureNode>();
    auto child3 = std::make_shared<MockSuccessNode>();

    sequence.addChild(child1);
    sequence.addChild(child2);
    sequence.addChild(child3);

    EXPECT_EQ(sequence.tick(), NodeStatus::FAILURE);
    EXPECT_EQ(child1->tickCount, 1);
    EXPECT_EQ(child2->tickCount, 1);
    EXPECT_EQ(child3->tickCount, 0); // Should not be reached
}

TEST_F(BehaviorTreeTest, SequenceNodeRunning) {
    SequenceNode sequence("TestSequence");
    auto child1 = std::make_shared<MockSuccessNode>();
    auto child2 = std::make_shared<MockRunningNode>();

    sequence.addChild(child1);
    sequence.addChild(child2);

    EXPECT_EQ(sequence.tick(), NodeStatus::RUNNING);
    EXPECT_EQ(child1->tickCount, 1);
    EXPECT_EQ(child2->tickCount, 1);
}

// ============================================================================
// SelectorNode Tests
// ============================================================================

TEST_F(BehaviorTreeTest, SelectorNodeAllFailure) {
    SelectorNode selector("TestSelector");
    auto child1 = std::make_shared<MockFailureNode>();
    auto child2 = std::make_shared<MockFailureNode>();

    selector.addChild(child1);
    selector.addChild(child2);

    EXPECT_EQ(selector.tick(), NodeStatus::FAILURE);
    EXPECT_EQ(child1->tickCount, 1);
    EXPECT_EQ(child2->tickCount, 1);
}

TEST_F(BehaviorTreeTest, SelectorNodeFirstSuccess) {
    SelectorNode selector("TestSelector");
    auto child1 = std::make_shared<MockSuccessNode>();
    auto child2 = std::make_shared<MockFailureNode>();

    selector.addChild(child1);
    selector.addChild(child2);

    EXPECT_EQ(selector.tick(), NodeStatus::SUCCESS);
    EXPECT_EQ(child1->tickCount, 1);
    EXPECT_EQ(child2->tickCount, 0); // Should not be reached
}

TEST_F(BehaviorTreeTest, SelectorNodeSecondSuccess) {
    SelectorNode selector("TestSelector");
    auto child1 = std::make_shared<MockFailureNode>();
    auto child2 = std::make_shared<MockSuccessNode>();

    selector.addChild(child1);
    selector.addChild(child2);

    EXPECT_EQ(selector.tick(), NodeStatus::SUCCESS);
    EXPECT_EQ(child1->tickCount, 1);
    EXPECT_EQ(child2->tickCount, 1);
}

// ============================================================================
// NodeStatus Tests
// ============================================================================

TEST_F(BehaviorTreeTest, NodeStatusValues) {
    EXPECT_EQ(static_cast<int>(NodeStatus::SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(NodeStatus::FAILURE), 1);
    EXPECT_EQ(static_cast<int>(NodeStatus::RUNNING), 2);
}
