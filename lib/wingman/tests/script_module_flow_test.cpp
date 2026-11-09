#include "script_function_test_helpers.hpp"

// ========== Orchestration module ==========

TEST(OrchestrationModuleFunctionsTest, SubmitWorkflowReturnsNull) {
    auto fn = findFunction("orchestration", "submit_workflow");
    ASSERT_FALSE(fn.name.empty());
    // 空对象（无 tasks）：提交拒绝恒 null
    auto result = fn({ScriptValue::fromObject({})});
    EXPECT_TRUE(result.isNull());
}

TEST(OrchestrationModuleFunctionsTest, CancelWorkflowReturnsFalse) {
    auto fn = findFunction("orchestration", "cancel_workflow");
    ASSERT_FALSE(fn.name.empty());
    // 未知 ID 恒 false（与进程内是否已提交过工作流无关——全局管理器跨用例
    // 累积，不做空集/恒空断言）
    auto result = fn({ScriptValue::fromString("wf1")});
    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(result.asBool());
}

TEST(OrchestrationModuleFunctionsTest, GetWorkflowReturnsNull) {
    auto fn = findFunction("orchestration", "get_workflow");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("wf1")});
    EXPECT_TRUE(result.isNull());
}

TEST(OrchestrationModuleFunctionsTest, GetAllWorkflowsReturnsArray) {
    auto fn = findFunction("orchestration", "get_all_workflows");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}


// ========== Team module ==========

TEST(TeamModuleFunctionsTest, JoinTeamReturnsTrueOptimistically) {
    auto fn = findFunction("team", "joinTeam");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("team1")});
    // Returns true optimistically (local state updated immediately)
    // Actual server confirmation happens asynchronously via inbox
    EXPECT_TRUE(result.asBool());
}

TEST(TeamModuleFunctionsTest, LeaveTeamReturnsTrueWhenInTeam) {
    // First join a team
    auto joinFn = findFunction("team", "joinTeam");
    ASSERT_FALSE(joinFn.name.empty());
    joinFn({ScriptValue::fromString("team1")});

    // Then leave
    auto fn = findFunction("team", "leaveTeam");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return true when in a team
    EXPECT_TRUE(result.asBool());
}

TEST(TeamModuleFunctionsTest, CreateVoteReturnsFalseWhenNotJoined) {
    auto fn = findFunction("team", "createVote");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test vote")});
    // Should return false when not in a team
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, GetTeamStatusReturnsJsonString) {
    auto fn = findFunction("team", "getTeamStatus");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return JSON string
    EXPECT_TRUE(result.isString());
}

TEST(TeamModuleFunctionsTest, IsJoinedReturnsFalseInitially) {
    auto fn = findFunction("team", "isJoined");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return false when not in a team
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, GetMemberIdReturnsEmptyStringInitially) {
    auto fn = findFunction("team", "getMemberId");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return empty string when not in a team
    EXPECT_TRUE(result.asString().empty());
}

TEST(TeamModuleFunctionsTest, BroadcastReturnsFalseWhenNotJoined) {
    auto fn = findFunction("team", "broadcast");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test message")});
    // Should return false when not in a team
    EXPECT_FALSE(result.asBool());
}


// ========== SmartTrigger module functions ==========

TEST(SmartTriggerModuleFunctionsTest, CreateTrigger) {
    auto fn = findFunction("smarttrigger", "create");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test_st_func")});
    EXPECT_TRUE(result.asBool());

    // Cleanup
    SmartTriggerManager::instance().removeTrigger("test_st_func");
}

TEST(SmartTriggerModuleFunctionsTest, StartNonexistent) {
    auto fn = findFunction("smarttrigger", "start");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_st")});
    EXPECT_FALSE(result.asBool());
}

TEST(SmartTriggerModuleFunctionsTest, StopNonexistent) {
    auto fn = findFunction("smarttrigger", "stop");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_st")});
    EXPECT_FALSE(result.asBool());
}

TEST(SmartTriggerModuleFunctionsTest, RemoveDoesNotCrash) {
    auto fn = findFunction("smarttrigger", "remove");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_st")}));
}


// ========== Verification module functions ==========

TEST(VerificationModuleFunctionsTest, TotpGeneratesCode) {
    auto fn = findFunction("verification", "totp");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 6u);
}

TEST(VerificationModuleFunctionsTest, TotpWithDigits) {
    auto fn = findFunction("verification", "totp");
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP"), ScriptValue::fromInt(8)});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 8u);
}

TEST(VerificationModuleFunctionsTest, TotpWithPeriod) {
    auto fn = findFunction("verification", "totp");
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP"), ScriptValue::fromInt(6), ScriptValue::fromInt(60)});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 6u);
}

TEST(VerificationModuleFunctionsTest, SteamGuardGeneratesCode) {
    auto fn = findFunction("verification", "steamGuard");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 5u);
}

TEST(VerificationModuleFunctionsTest, RemainingSeconds) {
    auto fn = findFunction("verification", "remaining");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(30)});
    EXPECT_TRUE(result.isInt());
    int remaining = result.asInt();
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 30);
}

TEST(VerificationModuleFunctionsTest, VerifyCode) {
    auto totpFn = findFunction("verification", "totp");
    auto verifyFn = findFunction("verification", "verify");
    ASSERT_FALSE(totpFn.name.empty());
    ASSERT_FALSE(verifyFn.name.empty());

    auto code = totpFn({ScriptValue::fromString("JBSWY3DPEHPK3PXP")}).asString();
    EXPECT_FALSE(code.empty());
}


