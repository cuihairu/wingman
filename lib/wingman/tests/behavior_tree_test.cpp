#include <gtest/gtest.h>
#include "wingman/behavior_tree.hpp"

using namespace wingman;

// ========== BehaviorContext ==========

TEST(BehaviorContextTest, StringVariables) {
    BehaviorContext ctx;
    ctx.variables["name"] = "test";
    EXPECT_FALSE(ctx.variables.empty());
    EXPECT_EQ(ctx.variables["name"], "test");
}

TEST(BehaviorContextTest, NumberVariables) {
    BehaviorContext ctx;
    ctx.numbers["count"] = 42.0;
    EXPECT_FALSE(ctx.numbers.empty());
    EXPECT_DOUBLE_EQ(ctx.numbers["count"], 42.0);
}

TEST(BehaviorContextTest, BoolFlags) {
    BehaviorContext ctx;
    ctx.flags["active"] = true;
    EXPECT_FALSE(ctx.flags.empty());
    EXPECT_TRUE(ctx.flags["active"]);
}

// ========== SequenceNode ==========

TEST(SequenceNodeTest, AllSuccess) {
    auto seq = std::make_shared<SequenceNode>();
    seq->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    seq->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(seq->tick(), NodeStatus::SUCCESS);
}

TEST(SequenceNodeTest, MiddleFailure) {
    auto seq = std::make_shared<SequenceNode>();
    seq->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    seq->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    seq->addChild(std::make_shared<ActionNode>("c", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(seq->tick(), NodeStatus::FAILURE);
}

TEST(SequenceNodeTest, EmptyReturnsSuccess) {
    auto seq = std::make_shared<SequenceNode>();
    EXPECT_EQ(seq->tick(), NodeStatus::SUCCESS);
}

TEST(SequenceNodeTest, RunningChild) {
    int callCount = 0;
    auto seq = std::make_shared<SequenceNode>();
    seq->addChild(std::make_shared<ActionNode>("a", [&](BehaviorContext&) {
        callCount++;
        return callCount < 2 ? NodeStatus::RUNNING : NodeStatus::SUCCESS;
    }));
    EXPECT_EQ(seq->tick(), NodeStatus::RUNNING);
    EXPECT_EQ(seq->tick(), NodeStatus::SUCCESS);
}

TEST(SequenceNodeTest, GetName) {
    SequenceNode node("MySeq");
    EXPECT_EQ(node.getName(), "MySeq");
}

// ========== SelectorNode ==========

TEST(SelectorNodeTest, FirstSuccess) {
    auto sel = std::make_shared<SelectorNode>();
    sel->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    sel->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(sel->tick(), NodeStatus::SUCCESS);
}

TEST(SelectorNodeTest, AllFailure) {
    auto sel = std::make_shared<SelectorNode>();
    sel->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    sel->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    EXPECT_EQ(sel->tick(), NodeStatus::FAILURE);
}

TEST(SelectorNodeTest, EmptyReturnsFailure) {
    auto sel = std::make_shared<SelectorNode>();
    EXPECT_EQ(sel->tick(), NodeStatus::FAILURE);
}

TEST(SelectorNodeTest, MiddleSuccess) {
    auto sel = std::make_shared<SelectorNode>();
    sel->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    sel->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    sel->addChild(std::make_shared<ActionNode>("c", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(sel->tick(), NodeStatus::SUCCESS);
}

// ========== ParallelNode ==========

TEST(ParallelNodeTest, SucceedOnAll) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::SUCCEED_ON_ALL);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(par->tick(), NodeStatus::SUCCESS);
}

TEST(ParallelNodeTest, SucceedOnAllPartialFailure) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::SUCCEED_ON_ALL);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    EXPECT_EQ(par->tick(), NodeStatus::FAILURE);
}

TEST(ParallelNodeTest, SucceedOnOne) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::SUCCEED_ON_ONE);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(par->tick(), NodeStatus::SUCCESS);
}

TEST(ParallelNodeTest, FailOnAll) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::FAIL_ON_ALL);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    EXPECT_EQ(par->tick(), NodeStatus::FAILURE);
}

TEST(ParallelNodeTest, FailOnAllNotAllFail) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::FAIL_ON_ALL);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(par->tick(), NodeStatus::SUCCESS);
}

TEST(ParallelNodeTest, FailOnOne) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::FAIL_ON_ONE);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(par->tick(), NodeStatus::FAILURE);
}

// ========== RepeatNode ==========

TEST(RepeatNodeTest, FiniteRepeat) {
    int count = 0;
    auto repeat = std::make_shared<RepeatNode>(
        std::make_shared<ActionNode>("act", [&](BehaviorContext&) {
            count++;
            return NodeStatus::SUCCESS;
        }),
        3
    );

    // Repeat 3 times: each tick the child succeeds, count increments, returns RUNNING
    // until count reaches 3
    NodeStatus status = NodeStatus::RUNNING;
    while (status == NodeStatus::RUNNING) {
        status = repeat->tick();
    }
    EXPECT_EQ(status, NodeStatus::SUCCESS);
    EXPECT_EQ(count, 3);
}

// ========== RetryNode ==========

TEST(RetryNodeTest, SuccessOnFirstTry) {
    auto retry = std::make_shared<RetryNode>(
        std::make_shared<ActionNode>("act", [](BehaviorContext&) { return NodeStatus::SUCCESS; }),
        3
    );
    EXPECT_EQ(retry->tick(), NodeStatus::SUCCESS);
}

TEST(RetryNodeTest, SuccessAfterRetries) {
    int attempt = 0;
    auto retry = std::make_shared<RetryNode>(
        std::make_shared<ActionNode>("act", [&](BehaviorContext&) {
            attempt++;
            return attempt >= 2 ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        }),
        3
    );
    EXPECT_EQ(retry->tick(), NodeStatus::SUCCESS);
    EXPECT_EQ(attempt, 2);
}

TEST(RetryNodeTest, ExhaustRetries) {
    auto retry = std::make_shared<RetryNode>(
        std::make_shared<ActionNode>("act", [](BehaviorContext&) { return NodeStatus::FAILURE; }),
        2
    );
    EXPECT_EQ(retry->tick(), NodeStatus::FAILURE);
}

// ========== InverterNode ==========

TEST(InverterNodeTest, InvertsSuccess) {
    auto inv = std::make_shared<InverterNode>(
        std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; })
    );
    EXPECT_EQ(inv->tick(), NodeStatus::FAILURE);
}

TEST(InverterNodeTest, InvertsFailure) {
    auto inv = std::make_shared<InverterNode>(
        std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; })
    );
    EXPECT_EQ(inv->tick(), NodeStatus::SUCCESS);
}

TEST(InverterNodeTest, PassesThroughRunning) {
    auto inv = std::make_shared<InverterNode>(
        std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::RUNNING; })
    );
    EXPECT_EQ(inv->tick(), NodeStatus::RUNNING);
}

// ========== ConditionNode ==========

TEST(ConditionNodeTest, TrueCondition) {
    ConditionNode cond("is_true", [](BehaviorContext&) { return true; });
    EXPECT_EQ(cond.tick(), NodeStatus::SUCCESS);
}

TEST(ConditionNodeTest, FalseCondition) {
    ConditionNode cond("is_false", [](BehaviorContext&) { return false; });
    EXPECT_EQ(cond.tick(), NodeStatus::FAILURE);
}

TEST(ConditionNodeTest, GetName) {
    ConditionNode cond("my_cond", [](BehaviorContext&) { return true; });
    EXPECT_EQ(cond.getName(), "my_cond");
}

// ========== CheckNode ==========

TEST(CheckNodeTest, Equal) {
    CheckNode check("x", 10.0, CheckNode::Op::EQUAL);
    EXPECT_EQ(check.getName(), "Check");
    // CheckNode uses a static context, so we just test it doesn't crash
    check.tick();
}

// ========== ActionNode ==========

TEST(ActionNodeTest, ReturnsSuccess) {
    ActionNode action("do_thing", [](BehaviorContext&) { return NodeStatus::SUCCESS; });
    EXPECT_EQ(action.tick(), NodeStatus::SUCCESS);
}

TEST(ActionNodeTest, ReturnsFailure) {
    ActionNode action("fail_thing", [](BehaviorContext&) { return NodeStatus::FAILURE; });
    EXPECT_EQ(action.tick(), NodeStatus::FAILURE);
}

// ========== WaitNode ==========

TEST(WaitNodeTest, FirstTickReturnsRunning) {
    WaitNode wait(5000);
    EXPECT_EQ(wait.tick(), NodeStatus::RUNNING);
}

// ========== DelayNode ==========

TEST(DelayNodeTest, FirstTickReturnsRunning) {
    auto delay = std::make_shared<DelayNode>(
        std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }),
        5000
    );
    EXPECT_EQ(delay->tick(), NodeStatus::RUNNING);
}

// ========== BehaviorTree ==========

TEST(BehaviorTreeTest, TickWithRoot) {
    BehaviorTree tree("test");
    tree.setRoot(std::make_shared<ActionNode>("root", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    EXPECT_EQ(tree.tick(), NodeStatus::SUCCESS);
}

TEST(BehaviorTreeTest, TickWithoutRootReturnsFailure) {
    BehaviorTree tree("empty");
    EXPECT_EQ(tree.tick(), NodeStatus::FAILURE);
}

TEST(BehaviorTreeTest, GetName) {
    BehaviorTree tree("my_tree");
    EXPECT_EQ(tree.getName(), "my_tree");
}

TEST(BehaviorTreeTest, ContextAccess) {
    BehaviorTree tree("ctx_test");
    auto& ctx = tree.getContext();
    ctx.variables["key"] = "value";
    EXPECT_EQ(ctx.variables["key"], "value");
}

TEST(BehaviorTreeTest, FactorySequence) {
    auto seq = BehaviorTree::sequence("my_seq");
    EXPECT_NE(seq, nullptr);
    EXPECT_EQ(seq->getName(), "my_seq");
}

TEST(BehaviorTreeTest, FactorySelector) {
    auto sel = BehaviorTree::selector("my_sel");
    EXPECT_NE(sel, nullptr);
}

TEST(BehaviorTreeTest, FactoryCondition) {
    auto cond = BehaviorTree::condition("check", []() { return true; });
    EXPECT_NE(cond, nullptr);
    EXPECT_EQ(cond->tick(), NodeStatus::SUCCESS);
}

TEST(BehaviorTreeTest, FactoryAction) {
    auto act = BehaviorTree::action("do", []() { return NodeStatus::SUCCESS; });
    EXPECT_NE(act, nullptr);
    EXPECT_EQ(act->tick(), NodeStatus::SUCCESS);
}

TEST(BehaviorTreeTest, ConstructWithRoot) {
    auto root = std::make_shared<ActionNode>("r", [](BehaviorContext&) { return NodeStatus::SUCCESS; });
    BehaviorTree tree("with_root", root);
    EXPECT_EQ(tree.tick(), NodeStatus::SUCCESS);
}

// ========== BehaviorTreeManager ==========

TEST(BehaviorTreeManagerTest, CreateAndGetTree) {
    auto& mgr = BehaviorTreeManager::instance();
    auto tree = mgr.createTree("test_tree_mgr");
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree->getName(), "test_tree_mgr");

    auto retrieved = mgr.getTree("test_tree_mgr");
    EXPECT_EQ(retrieved, tree);

    mgr.removeTree("test_tree_mgr");
    EXPECT_EQ(mgr.getTree("test_tree_mgr"), nullptr);
}

TEST(BehaviorTreeManagerTest, GetAllTrees) {
    auto& mgr = BehaviorTreeManager::instance();
    mgr.createTree("tree_a");
    mgr.createTree("tree_b");

    auto all = mgr.getAllTrees();
    EXPECT_GE(all.size(), 2u);

    mgr.removeTree("tree_a");
    mgr.removeTree("tree_b");
}

TEST(BehaviorTreeManagerTest, GetNonExistentReturnsNull) {
    auto& mgr = BehaviorTreeManager::instance();
    EXPECT_EQ(mgr.getTree("nonexistent"), nullptr);
}

TEST(BehaviorTreeManagerTest, RemoveNonExistentDoesNotCrash) {
    auto& mgr = BehaviorTreeManager::instance();
    EXPECT_NO_THROW(mgr.removeTree("nonexistent"));
}

TEST(BehaviorTreeManagerTest, CreateDuplicateReturnsExisting) {
    auto& mgr = BehaviorTreeManager::instance();
    auto tree1 = mgr.createTree("dup_tree");
    auto tree2 = mgr.createTree("dup_tree");
    EXPECT_EQ(tree1, tree2);
    mgr.removeTree("dup_tree");
}
