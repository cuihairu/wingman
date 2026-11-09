#include "script_function_test_helpers.hpp"

// ========== Performance module functions (stubs) ==========

TEST(PerformanceModuleFunctionsTest, SetConfigDoesNotCrash) {
    auto fn = findFunction("perf", "setConfig");
    ASSERT_FALSE(fn.name.empty());
    auto config = ScriptValue::fromObject({
        {"enableImageCache", ScriptValue::fromBool(true)},
        {"maxCacheSize", ScriptValue::fromInt(100)},
        {"enableParallelProcessing", ScriptValue::fromBool(false)},
        {"numThreads", ScriptValue::fromInt(4)}
    });
    auto result = fn({config});
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, GetConfigReturnsObject) {
    auto fn = findFunction("perf", "getConfig");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(PerformanceModuleFunctionsTest, PreloadImageReturnsNull) {
    auto fn = findFunction("perf", "preloadImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.png")});
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, ClearCacheReturnsNull) {
    auto fn = findFunction("perf", "clearCache");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, GetCacheSizeReturnsInt) {
    auto fn = findFunction("perf", "getCacheSize");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(PerformanceModuleFunctionsTest, GetCacheStatsReturnsObject) {
    auto fn = findFunction("perf", "getCacheStats");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(PerformanceModuleFunctionsTest, FastFindImageReturnsNull) {
    auto fn = findFunction("perf", "fastFindImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({
        ScriptValue::fromString("img.png"),
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromFloat(0.9)
    });
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, ParallelFindColorsReturnsArray) {
    auto fn = findFunction("perf", "parallelFindColors");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({
        ScriptValue::fromInt(0xFF0000),
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromInt(10)
    });
    EXPECT_TRUE(result.isArray());
}

TEST(PerformanceModuleFunctionsTest, GetStatsReturnsObject) {
    auto fn = findFunction("perf", "getStats");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(PerformanceModuleFunctionsTest, ResetStatsReturnsNull) {
    auto fn = findFunction("perf", "resetStats");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}


// ========== ML module functions (optional backend) ==========

TEST(MlModuleFunctionsTest, ProvidersReturnsArray) {
    auto fn = findFunction("ml", "providers");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
    EXPECT_GE(result.size(), 1u);
}

TEST(MlModuleFunctionsTest, InvalidModelQueriesReturnSafeDefaults) {
    auto isLoaded = findFunction("ml", "isLoaded");
    auto inputs = findFunction("ml", "inputs");
    auto outputs = findFunction("ml", "outputs");
    auto unload = findFunction("ml", "unload");
    ASSERT_FALSE(isLoaded.name.empty());
    ASSERT_FALSE(inputs.name.empty());
    ASSERT_FALSE(outputs.name.empty());
    ASSERT_FALSE(unload.name.empty());

    const auto missingId = ScriptValue::fromString("missing-model");

    auto loadedResult = isLoaded({missingId});
    EXPECT_TRUE(loadedResult.isBool());
    EXPECT_FALSE(loadedResult.asBool());

    auto inputsResult = inputs({missingId});
    EXPECT_TRUE(inputsResult.isArray());
    EXPECT_EQ(inputsResult.size(), 0u);

    auto outputsResult = outputs({missingId});
    EXPECT_TRUE(outputsResult.isArray());
    EXPECT_EQ(outputsResult.size(), 0u);

    auto unloadResult = unload({missingId});
    EXPECT_TRUE(unloadResult.isBool());
    EXPECT_FALSE(unloadResult.asBool());
}

TEST(MlModuleFunctionsTest, LoadModelWithoutArgsReturnsNull) {
    auto fn = findFunction("ml", "loadModel");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}


// ========== Debugger module functions (stubs) ==========

TEST(DebuggerModuleFunctionsTest, StartReturnsFalse) {
    auto fn = findFunction("debugger", "start");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(result.asBool());
}

TEST(DebuggerModuleFunctionsTest, StopReturnsNull) {
    auto fn = findFunction("debugger", "stop");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

TEST(DebuggerModuleFunctionsTest, BreakpointReturnsString) {
    auto fn = findFunction("debugger", "breakpoint");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test.lua"), ScriptValue::fromInt(42)});
    EXPECT_TRUE(result.isString());
    EXPECT_NE(result.asString().find("test.lua"), std::string::npos);
    EXPECT_NE(result.asString().find("42"), std::string::npos);
}

TEST(DebuggerModuleFunctionsTest, BreakHereReturnsString) {
    auto fn = findFunction("debugger", "breakHere");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString(), "DEBUG_BREAK_HERE");
}


// ========== HTTP module functions (stubs) ==========

TEST(HttpModuleFunctionsTest, GetReturnsObject) {
    auto fn = findFunction("http", "get");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1")});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, GetWithOptions) {
    auto fn = findFunction("http", "get");
    auto opts = ScriptValue::fromObject({
        {"timeout", ScriptValue::fromInt(1)},
        {"followRedirects", ScriptValue::fromBool(false)},
        {"maxRedirects", ScriptValue::fromInt(0)},
        {"headers", ScriptValue::fromObject({
            {"X-Test", ScriptValue::fromString("value")}
        })}
    });
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1"), opts});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, PostReturnsObject) {
    auto fn = findFunction("http", "post");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1"), ScriptValue::fromString("{}")});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, PutReturnsObject) {
    auto fn = findFunction("http", "put");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1"), ScriptValue::fromString("{}")});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, DelReturnsObject) {
    auto fn = findFunction("http", "delete");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1")});
    EXPECT_TRUE(result.isObject());
}


// ========== Util module functions ==========

TEST(UtilModuleFunctionsTest, GetTimeReturnsInt) {
    auto fn = findFunction("util", "getTime");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(UtilModuleFunctionsTest, SleepDoesNotCrash) {
    auto fn = findFunction("util", "sleep");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(1)}));
}

TEST(UtilModuleFunctionsTest, LogDoesNotCrash) {
    auto fn = findFunction("util", "log");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("test message")}));
}


// ========== Node module functions ==========

TEST(NodeModuleFunctionsTest, CreateHeartbeatReturnsObject) {
    auto fn = findFunction("node", "createHeartbeat");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(NodeModuleFunctionsTest, SendHeartbeatReturnsNull) {
    auto fn = findFunction("node", "sendHeartbeat");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

TEST(NodeModuleFunctionsTest, GetWindowsReturnsArray) {
    auto fn = findFunction("node", "getWindows");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}


// ========== Module presence verification ==========

TEST(ModulePresenceTest, AllExpectedModulesExist) {
    std::vector<std::string> expectedModules = {
        "json", "system", "orchestration", "team",
        "smarttrigger", "bt", "ocr",
        "screen", "input", "window", "vision",
        "http", "human", "verification", "gameprofile",
        "perf", "config", "kv", "debugger",
        "util", "node", "uia", "process", "security", "ml"
    };

    for (const auto& name : expectedModules) {
        EXPECT_TRUE(hasModule(name)) << "Module '" << name << "' not found";
    }
}

TEST(ModulePresenceTest, ModuleFunctionsAreCallable) {
    // This is a conservative smoke test for low-risk functions only. Modules
    // with real I/O, background state, input events, or singleton mutations have
    // dedicated tests and are excluded to avoid cross-test pollution.
    static const std::set<std::string> smokeModules = {
        "debugger",
        "json",
        "node",
        "orchestration",
        "util",
        "verification"
    };

    // Functions that perform blocking I/O (network requests, process waiting)
    // or require specific runtime state and must be skipped in this smoke test.
    static const std::set<std::pair<std::string, std::string>> blockedFunctions = {
        // HTTP — performs network I/O with timeout
        {"http", "get"},
        {"http", "post"},
        {"http", "put"},
        {"http", "delete"},
        // Process — waits/polls for external processes
        {"process", "wait"},
        {"process", "waitFor"},
        {"process", "start"},
        // Window — polls for external window state
        {"window", "waitFor"},
        // Task — waits on condition variable with timeout
        {"task", "wait"},
        {"task", "submit"},
        {"task", "retry"},
        // Notify — webhook performs network I/O
        {"notify", "webhook"},
        {"notify", "bridge"},
        // Event — registers persistent callbacks
        {"event", "on"},
        {"event", "once"},
        // Config/game profile — mutate repository files or singleton profile state
        {"config", "save"},
        {"config", "set"},
        {"config", "remove"},
        {"gameprofile", "save"},
        {"gameprofile", "setActive"},
        {"gameprofile", "setProfilesDirectory"},
        {"gameprofile", "scan"},
        {"gameprofile", "createTemplate"},
        {"gameprofile", "importJson"},
        {"gameprofile", "exportPackage"},
        {"gameprofile", "importPackage"},
        {"gameprofile", "delete"},
    };

    auto modules = getAllModules();
    for (const auto& mod : modules) {
        if (!smokeModules.count(mod.name)) continue;
        for (const auto& fn : mod.functions) {
            if (blockedFunctions.count({mod.name, fn.name})) continue;
            std::vector<ScriptValue> args;
            // Build minimal args from signature to avoid UB on empty vector access.
            // Count parameters by counting commas before "->", then provide that many defaults.
            const auto& sig = fn.signature;
            auto arrowPos = sig.find(" -> ");
            std::string paramsPart = (arrowPos != std::string::npos) ? sig.substr(0, arrowPos) : sig;
            bool hasParams = !paramsPart.empty() && paramsPart != "()";
            if (hasParams) {
                // Count commas to determine parameter count
                int paramCount = 1;
                for (char c : paramsPart) {
                    if (c == ',') paramCount++;
                }
                // Determine first param type for the first arg
                auto colonPos = paramsPart.find(':');
                if (colonPos != std::string::npos && paramsPart.substr(colonPos, 7) == ":string") {
                    args.push_back(ScriptValue::fromString("test"));
                } else {
                    args.push_back(ScriptValue::fromInt(1));
                }
                // Fill remaining args with int defaults
                for (int i = 1; i < paramCount; ++i) {
                    args.push_back(ScriptValue::fromInt(1));
                }
            }
            EXPECT_NO_THROW(fn.func(args))
                << "Module '" << mod.name << "' function '" << fn.name << "' threw";
        }
    }
}

TEST(HumanModuleFunctionsTest, SetDelayRangeRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setDelayRange");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromInt(50), ScriptValue::fromInt(200)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_EQ(cfg.get("delay_min")->asInt(), 50);
    EXPECT_EQ(cfg.get("delay_max")->asInt(), 200);
}

TEST(HumanModuleFunctionsTest, SetMoveSpeedRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setMoveSpeed");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromFloat(1.5)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_NEAR(cfg.get("move_speed")->asFloat(), 1.5, 1e-9);
}

TEST(HumanModuleFunctionsTest, SetTypingVarianceRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setTypingVariance");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromFloat(0.3)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_NEAR(cfg.get("typing_variance")->asFloat(), 0.3, 1e-9);
}

TEST(HumanModuleFunctionsTest, SetMoveSpeedAffectsBackendMoveDuration) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    // 验证映射应用：move_speed=2.0 -> minMoveDuration=50, maxMoveDuration=150
    auto setFn = findFunction("human", "setMoveSpeed");
    ASSERT_FALSE(setFn.name.empty());
    setFn({ScriptValue::fromFloat(2.0)});
    auto mc = Human::mouse().getConfig();
    EXPECT_EQ(mc.minMoveDuration, 50); // 100/2
    EXPECT_EQ(mc.maxMoveDuration, 150); // 300/2
}

TEST(HumanModuleFunctionsTest, RandomDelayExplicitRangeDoesNotCrash) {
    auto fn = findFunction("human", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    // 显式传 0-1ms，避免测试耗时（默认 100-300ms 会真实 sleep）
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(0), ScriptValue::fromInt(1)}));
}

TEST(HumanModuleFunctionsTest, RandomDelayNoArgsUsesDefaultRange) {
    // 无参调用走默认 100-300ms 路径（仅验证可调用，接受单次 ~300ms sleep）
    auto fn = findFunction("human", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({}));
}

TEST(HumanModuleFunctionsTest, NaturalTypeDoesNotCrash) {
    auto fn = findFunction("human", "naturalType");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hi")}));
}

TEST(HumanModuleFunctionsTest, MoveMouseWithDefaultDurationDoesNotCrash) {
    auto fn = findFunction("human", "moveMouse");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(50), ScriptValue::fromInt(50)
    }));
}

TEST(HumanModuleFunctionsTest, MoveMouseExplicitDurationDoesNotCrash) {
    auto fn = findFunction("human", "moveMouse");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromInt(1) // 1ms，避免测试耗时
    }));
}

TEST(HumanModuleFunctionsTest, NaturalClickLeftDefaultDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)})); // 默认 left
}

TEST(HumanModuleFunctionsTest, NaturalClickRightButtonDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromString("right")}));
}

TEST(HumanModuleFunctionsTest, NaturalClickMiddleButtonDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromString("middle")}));
}


