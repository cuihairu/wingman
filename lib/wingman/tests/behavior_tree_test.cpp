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

// ========== BehaviorContext get<T> template specializations ==========

TEST(BehaviorContextTest, GetStringReturnsValue) {
    BehaviorContext ctx;
    ctx.variables["key"] = "hello";
    EXPECT_EQ(ctx.get<std::string>("key", "default"), "hello");
}

TEST(BehaviorContextTest, GetStringReturnsDefault) {
    BehaviorContext ctx;
    EXPECT_EQ(ctx.get<std::string>("missing", "default"), "default");
}

TEST(BehaviorContextTest, GetDoubleReturnsValue) {
    BehaviorContext ctx;
    ctx.numbers["val"] = 3.14;
    EXPECT_DOUBLE_EQ(ctx.get<double>("val", 0.0), 3.14);
}

TEST(BehaviorContextTest, GetDoubleReturnsDefault) {
    BehaviorContext ctx;
    EXPECT_DOUBLE_EQ(ctx.get<double>("missing", 1.0), 1.0);
}

TEST(BehaviorContextTest, GetBoolReturnsValue) {
    BehaviorContext ctx;
    ctx.flags["flag"] = true;
    EXPECT_TRUE(ctx.get<bool>("flag", false));
}

TEST(BehaviorContextTest, GetBoolReturnsDefault) {
    BehaviorContext ctx;
    EXPECT_FALSE(ctx.get<bool>("missing", false));
}

// ========== Sequence/Selector reset ==========

TEST(SequenceNodeTest, ResetDoesNotCrash) {
    auto seq = std::make_shared<SequenceNode>();
    seq->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    seq->tick();
    EXPECT_NO_THROW(seq->reset());
}

TEST(SelectorNodeTest, ResetDoesNotCrash) {
    auto sel = std::make_shared<SelectorNode>();
    sel->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::FAILURE; }));
    sel->tick();
    EXPECT_NO_THROW(sel->reset());
}

TEST(SelectorNodeTest, GetName) {
    SelectorNode sel("MySel");
    EXPECT_EQ(sel.getName(), "MySel");
}

// ========== CheckNode all ops ==========

TEST(CheckNodeTest, NotEqual) {
    CheckNode check("x", 10.0, CheckNode::Op::NOT_EQUAL);
    EXPECT_EQ(check.getName(), "Check");
    check.tick();
}

TEST(CheckNodeTest, Greater) {
    CheckNode check("x", 5.0, CheckNode::Op::GREATER);
    check.tick();
}

TEST(CheckNodeTest, Less) {
    CheckNode check("x", 5.0, CheckNode::Op::LESS);
    check.tick();
}

TEST(CheckNodeTest, GreaterEqual) {
    CheckNode check("x", 5.0, CheckNode::Op::GREATER_EQUAL);
    check.tick();
}

TEST(CheckNodeTest, LessEqual) {
    CheckNode check("x", 5.0, CheckNode::Op::LESS_EQUAL);
    check.tick();
}

// ========== ParallelNode edge cases ==========

TEST(ParallelNodeTest, SucceedOnAllWithRunning) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::SUCCEED_ON_ALL);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::RUNNING; }));
    EXPECT_EQ(par->tick(), NodeStatus::RUNNING);
}

TEST(ParallelNodeTest, SucceedOnOneWithRunning) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::SUCCEED_ON_ONE);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::RUNNING; }));
    par->addChild(std::make_shared<ActionNode>("b", [](BehaviorContext&) { return NodeStatus::RUNNING; }));
    EXPECT_EQ(par->tick(), NodeStatus::RUNNING);
}

TEST(ParallelNodeTest, FailOnAllWithRunning) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::FAIL_ON_ALL);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::RUNNING; }));
    EXPECT_EQ(par->tick(), NodeStatus::RUNNING);
}

TEST(ParallelNodeTest, FailOnOneWithRunning) {
    auto par = std::make_shared<ParallelNode>("", ParallelNode::Policy::FAIL_ON_ONE);
    par->addChild(std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::RUNNING; }));
    EXPECT_EQ(par->tick(), NodeStatus::RUNNING);
}

// ========== RepeatNode edge cases ==========

TEST(RepeatNodeTest, ResetDoesNotCrash) {
    auto repeat = std::make_shared<RepeatNode>(
        std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }),
        3
    );
    repeat->tick();
    EXPECT_NO_THROW(repeat->reset());
}

TEST(RepeatNodeTest, InfiniteRepeat) {
    int count = 0;
    auto repeat = std::make_shared<RepeatNode>(
        std::make_shared<ActionNode>("a", [&](BehaviorContext&) { count++; return NodeStatus::SUCCESS; }),
        0  // infinite
    );
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(repeat->tick(), NodeStatus::RUNNING);
    }
    EXPECT_EQ(count, 5);
}

// ========== RetryNode edge cases ==========

TEST(RetryNodeTest, ResetDoesNotCrash) {
    auto retry = std::make_shared<RetryNode>(
        std::make_shared<ActionNode>("a", [](BehaviorContext&) { return NodeStatus::SUCCESS; }),
        3
    );
    retry->tick();
    EXPECT_NO_THROW(retry->reset());
}

TEST(RetryNodeTest, RunningChild) {
    int attempt = 0;
    auto retry = std::make_shared<RetryNode>(
        std::make_shared<ActionNode>("a", [&](BehaviorContext&) {
            attempt++;
            return NodeStatus::RUNNING;
        }),
        3
    );
    EXPECT_EQ(retry->tick(), NodeStatus::RUNNING);
    EXPECT_EQ(attempt, 1);
}

// ========== BehaviorTree::reset ==========

TEST(BehaviorTreeTest, ResetDoesNotCrash) {
    BehaviorTree tree("reset_test");
    tree.setRoot(std::make_shared<ActionNode>("root", [](BehaviorContext&) { return NodeStatus::SUCCESS; }));
    tree.tick();
    EXPECT_NO_THROW(tree.reset());
}

TEST(BehaviorTreeTest, ResetWithoutRootDoesNotCrash) {
    BehaviorTree tree("empty_reset");
    EXPECT_NO_THROW(tree.reset());
}

// ========== Factory methods ==========

TEST(BehaviorTreeTest, FactoryParallel) {
    auto par = BehaviorTree::parallel("my_par", ParallelNode::Policy::SUCCEED_ON_ALL);
    EXPECT_NE(par, nullptr);
    EXPECT_EQ(par->getName(), "my_par");
}

TEST(BehaviorTreeTest, FactorySequenceEmptyName) {
    auto seq = BehaviorTree::sequence("");
    EXPECT_NE(seq, nullptr);
    EXPECT_EQ(seq->getName(), "Sequence");
}

TEST(BehaviorTreeTest, FactorySelectorEmptyName) {
    auto sel = BehaviorTree::selector("");
    EXPECT_NE(sel, nullptr);
    EXPECT_EQ(sel->getName(), "Selector");
}

TEST(BehaviorTreeTest, FactoryParallelEmptyName) {
    auto par = BehaviorTree::parallel("");
    EXPECT_NE(par, nullptr);
    EXPECT_EQ(par->getName(), "Parallel");
}

// ========== BehaviorTreeManager startAll/stopAll ==========

TEST(BehaviorTreeManagerTest, StartAllDoesNotCrash) {
    auto& mgr = BehaviorTreeManager::instance();
    EXPECT_NO_THROW(mgr.startAll());
}

TEST(BehaviorTreeManagerTest, StopAllDoesNotCrash) {
    auto& mgr = BehaviorTreeManager::instance();
    EXPECT_NO_THROW(mgr.stopAll());
}
