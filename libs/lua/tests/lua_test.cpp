/**
 * Wingman Lua Tests
 * Lua 引擎模块单元测试
 */

#include <gtest/gtest.h>
#include "wingman/lua/lua_engine.hpp"

using namespace wingman::lua;

// ========== LuaEngine 测试 ==========

TEST(LuaEngineTest, CreateEngine) {
    LuaEngine engine;
    EXPECT_NE(engine.getState(), nullptr);
}

TEST(LuaEngineTest, ExecuteString) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    EXPECT_TRUE(engine.executeString("return 1 + 1"));
    EXPECT_TRUE(engine.executeString("x = 42"));

    engine.shutdown();
}

TEST(LuaEngineTest, SetGlobalInteger) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.setGlobal("testVar", 123);

    lua_State* L = engine.getState();
    lua_getglobal(L, "testVar");
    int value = lua_tointeger(L, -1);
    lua_pop(L, 1);

    EXPECT_EQ(value, 123);

    engine.shutdown();
}

TEST(LuaEngineTest, SetGlobalString) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.setGlobal("testStr", "hello");

    lua_State* L = engine.getState();
    lua_getglobal(L, "testStr");
    const char* value = lua_tostring(L, -1);
    lua_pop(L, 1);

    EXPECT_STREQ(value, "hello");

    engine.shutdown();
}

TEST(LuaEngineTest, SetGlobalNumber) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.setGlobal("testNum", 3.14);

    lua_State* L = engine.getState();
    lua_getglobal(L, "testNum");
    double value = lua_tonumber(L, -1);
    lua_pop(L, 1);

    EXPECT_DOUBLE_EQ(value, 3.14);

    engine.shutdown();
}

TEST(LuaEngineTest, ExecuteInvalidSyntax) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    EXPECT_FALSE(engine.executeString("function invalid syntax"));

    engine.shutdown();
}

TEST(LuaEngineTest, ExecuteBuffer) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    const char* code = "return 2 * 3";
    EXPECT_TRUE(engine.executeBuffer(code, strlen(code), "test_buffer"));

    engine.shutdown();
}

TEST(LuaEngineTest, RegisterFunction) {
    LuaEngine engine;
    ASSERT_TRUE(engine.initialize());

    // 注册简单的测试函数
    engine.registerFunction("test_func", [](lua_State* L) -> int {
        lua_pushinteger(L, 42);
        return 1;
    });

    // 调用注册的函数
    EXPECT_TRUE(engine.executeString("result = test_func()"));

    lua_State* L = engine.getState();
    lua_getglobal(L, "result");
    int result = lua_tointeger(L, -1);
    lua_pop(L, 1);

    EXPECT_EQ(result, 42);

    engine.shutdown();
}
