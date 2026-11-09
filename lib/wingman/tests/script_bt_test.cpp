#include "script_function_test_helpers.hpp"

// ========== BehaviorTree module functions ==========

TEST(BtModuleFunctionsTest, CreateTree) {
    auto fn = findFunction("bt", "create");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test_bt_func")});
    EXPECT_TRUE(result.asBool());

    // Cleanup
    BehaviorTreeManager::instance().removeTree("test_bt_func");
}

TEST(BtModuleFunctionsTest, TickNonexistent) {
    auto fn = findFunction("bt", "tick");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_bt")});
    EXPECT_EQ(result.asString(), "FAILURE");
}

TEST(BtModuleFunctionsTest, RemoveDoesNotCrash) {
    auto fn = findFunction("bt", "remove");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_bt")}));
}


// ========== Plan 7: bt 节点构造脚本桥接 ==========

TEST(BtModuleFunctionsTest, SequenceReturnsPositiveHandle) {
    auto fn = findFunction("bt", "sequence");
    ASSERT_FALSE(fn.name.empty());
    auto h = fn({ScriptValue::fromString("seq1")});
    ASSERT_TRUE(h.isInt());
    EXPECT_GT(h.asInt(), 0); // 有效句柄 ≥ 1
}

TEST(BtModuleFunctionsTest, SelectorReturnsPositiveHandle) {
    auto fn = findFunction("bt", "selector");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({ScriptValue::fromString("sel1")}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, SequenceDefaultNameDoesNotCrash) {
    auto fn = findFunction("bt", "sequence");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({}).asInt(), 0); // 无参 → 默认名
}

TEST(BtModuleFunctionsTest, ParallelWithPolicyStringReturnsHandle) {
    auto fn = findFunction("bt", "parallel");
    ASSERT_FALSE(fn.name.empty());
    auto h = fn({ScriptValue::fromString("par"), ScriptValue::fromString("FAIL_ON_ONE")});
    EXPECT_GT(h.asInt(), 0);
}

TEST(BtModuleFunctionsTest, ParallelDefaultPolicyReturnsHandle) {
    auto fn = findFunction("bt", "parallel");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({}).asInt(), 0); // 无参 → 默认 SUCCEED_ON_ALL
}

TEST(BtModuleFunctionsTest, WaitReturnsPositiveHandle) {
    auto fn = findFunction("bt", "wait");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({ScriptValue::fromInt(100)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, InverterWithChildReturnsHandle) {
    auto waitFn = findFunction("bt", "wait");
    auto invFn = findFunction("bt", "inverter");
    ASSERT_FALSE(invFn.name.empty());
    int64_t child = waitFn({ScriptValue::fromInt(50)}).asInt();
    ASSERT_GT(child, 0);
    EXPECT_GT(invFn({ScriptValue::fromInt(child)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, InverterInvalidChildReturnsZero) {
    auto fn = findFunction("bt", "inverter");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromInt(999999)}).asInt(), 0); // 无效 child handle → 0
}

TEST(BtModuleFunctionsTest, RepeatWithCountReturnsHandle) {
    auto waitFn = findFunction("bt", "wait");
    auto repFn = findFunction("bt", "repeat");
    ASSERT_FALSE(repFn.name.empty());
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    ASSERT_GT(child, 0);
    EXPECT_GT(repFn({ScriptValue::fromInt(child), ScriptValue::fromInt(3)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, AddChildToSequenceReturnsTrue) {
    auto seqFn = findFunction("bt", "sequence");
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    ASSERT_FALSE(addFn.name.empty());
    int64_t parent = seqFn({}).asInt();
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    ASSERT_TRUE(addFn({ScriptValue::fromInt(parent), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildToParallelReturnsTrue) {
    auto parFn = findFunction("bt", "parallel");
    auto seqFn = findFunction("bt", "sequence");
    auto addFn = findFunction("bt", "addChild");
    int64_t parent = parFn({}).asInt();
    int64_t child = seqFn({}).asInt(); // 复合节点也可作 child
    EXPECT_TRUE(addFn({ScriptValue::fromInt(parent), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildInvalidParentReturnsFalse) {
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    EXPECT_FALSE(addFn({ScriptValue::fromInt(999999), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildToLeafReturnsFalse) {
    // 叶子/装饰节点不支持 addChild（仅复合节点 Sequence/Selector/Parallel 支持）
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    int64_t leaf = waitFn({ScriptValue::fromInt(10)}).asInt();
    int64_t another = waitFn({ScriptValue::fromInt(20)}).asInt();
    EXPECT_FALSE(addFn({ScriptValue::fromInt(leaf), ScriptValue::fromInt(another)}).asBool());
}

TEST(BtModuleFunctionsTest, ConditionWithCallableReturnsHandle) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    // callable 返回 bool true（模拟脚本条件函数）
    ScriptValue cb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(true);
    });
    auto h = fn({ScriptValue::fromString("hp_low"), cb});
    EXPECT_GT(h.asInt(), 0);
}

TEST(BtModuleFunctionsTest, ConditionInvalidCallableReturnsZero) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    // 第二参非 callable → 返回 0
    EXPECT_EQ(fn({ScriptValue::fromString("bad"), ScriptValue::fromInt(123)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, ConditionMissingCallableReturnsZero) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromString("nocb")}).asInt(), 0); // 缺 callable
}

TEST(BtModuleFunctionsTest, ActionWithCallableReturnsHandle) {
    auto fn = findFunction("bt", "action");
    ASSERT_FALSE(fn.name.empty());
    ScriptValue cb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    EXPECT_GT(fn({ScriptValue::fromString("attack"), cb}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, ActionInvalidCallableReturnsZero) {
    auto fn = findFunction("bt", "action");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromString("bad"), ScriptValue::fromInt(1)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, SetRootOnExistingTreeReturnsTrue) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto setRootFn = findFunction("bt", "setRoot");
    ASSERT_FALSE(setRootFn.name.empty());
    std::string tree = "plan7_setroot_test";
    createFn({ScriptValue::fromString(tree)});
    int64_t node = seqFn({}).asInt();
    ASSERT_TRUE(setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(node)}).asBool());
    findFunction("bt", "remove")({ScriptValue::fromString(tree)});
}

TEST(BtModuleFunctionsTest, SetRootInvalidTreeReturnsFalse) {
    auto seqFn = findFunction("bt", "sequence");
    auto setRootFn = findFunction("bt", "setRoot");
    int64_t node = seqFn({}).asInt();
    EXPECT_FALSE(setRootFn({ScriptValue::fromString("no_such_tree_xyz"), ScriptValue::fromInt(node)}).asBool());
}

// 端到端：condition(true) + action(SUCCESS) 组装成 sequence，setRoot，tick → SUCCESS
TEST(BtModuleFunctionsTest, EndToEndConditionActionTickReturnsSuccess) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto condFn = findFunction("bt", "condition");
    auto actFn = findFunction("bt", "action");
    auto addFn = findFunction("bt", "addChild");
    auto setRootFn = findFunction("bt", "setRoot");
    auto tickFn = findFunction("bt", "tick");
    auto removeFn = findFunction("bt", "remove");

    std::string tree = "plan7_e2e_test";
    ASSERT_TRUE(createFn({ScriptValue::fromString(tree)}).asBool());

    ScriptValue condCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(true);
    });
    ScriptValue actCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    int64_t cond = condFn({ScriptValue::fromString("ready"), condCb}).asInt();
    int64_t act = actFn({ScriptValue::fromString("go"), actCb}).asInt();
    int64_t seq = seqFn({}).asInt();
    ASSERT_GT(cond, 0); ASSERT_GT(act, 0); ASSERT_GT(seq, 0);

    ASSERT_TRUE(addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(cond)}).asBool());
    ASSERT_TRUE(addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)}).asBool());
    ASSERT_TRUE(setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(seq)}).asBool());

    EXPECT_EQ(tickFn({ScriptValue::fromString(tree)}).asString(), "SUCCESS");
    removeFn({ScriptValue::fromString(tree)});
}

// 端到端：condition(false) → sequence 失败 → tick → FAILURE
TEST(BtModuleFunctionsTest, EndToEndFalseConditionTickReturnsFailure) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto condFn = findFunction("bt", "condition");
    auto actFn = findFunction("bt", "action");
    auto addFn = findFunction("bt", "addChild");
    auto setRootFn = findFunction("bt", "setRoot");
    auto tickFn = findFunction("bt", "tick");
    auto removeFn = findFunction("bt", "remove");

    std::string tree = "plan7_e2e_fail_test";
    createFn({ScriptValue::fromString(tree)});

    ScriptValue condCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(false);
    });
    ScriptValue actCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    int64_t cond = condFn({ScriptValue::fromString("ready"), condCb}).asInt();
    int64_t act = actFn({ScriptValue::fromString("go"), actCb}).asInt();
    int64_t seq = seqFn({}).asInt();
    addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(cond)});
    addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)});
    setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(seq)});

    EXPECT_EQ(tickFn({ScriptValue::fromString(tree)}).asString(), "FAILURE");
    removeFn({ScriptValue::fromString(tree)});
}


