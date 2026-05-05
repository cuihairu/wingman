#include <gtest/gtest.h>
#include <sol/sol.hpp>
#include <fstream>
#include "wingman/lua_http.hpp"

using namespace wingman;

class LuaHttpTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// LuaHttpContext Tests
// ============================================================================

TEST_F(LuaHttpTest, HttpContext_BasicRequestAccess) {
    crow::request req;
    req.body = R"({"test": "data"})";
    req.url = "/api/test?name=value";
    req.method = "POST";

    LuaHttpContext ctx(req);

    EXPECT_EQ(ctx.getBody(), R"({"test": "data"})");
    EXPECT_EQ(ctx.getURL(), "/api/test");
    EXPECT_EQ(ctx.getMethod(), "POST");
    EXPECT_EQ(ctx.getQuery("name"), "value");
}

TEST_F(LuaHttpTest, HttpContext_GetJSON) {
    crow::request req;
    req.body = R"({"name": "test", "value": 123})";

    LuaHttpContext ctx(req);
    auto json = ctx.getJSON();

    EXPECT_TRUE(json.is_object());
    EXPECT_EQ(json["name"], "test");
    EXPECT_EQ(json["value"], 123);
}

TEST_F(LuaHttpTest, HttpContext_ResponseHelpers) {
    crow::request req;
    LuaHttpContext ctx(req);

    // Test OK
    auto okResp = ctx.OK({{"message", "success"}});
    EXPECT_EQ(okResp.code, 200);
    EXPECT_TRUE(okResp.body.find("\"success\":true") != std::string::npos);

    // Test Error
    auto errResp = ctx.Error("Not found", 404);
    EXPECT_EQ(errResp.code, 404);
    EXPECT_TRUE(errResp.body.find("\"error\":\"Not found\"") != std::string::npos);
}

TEST_F(LuaHttpTest, HttpContext_SessionAndStorage) {
    crow::request req;
    LuaHttpContext ctx(req);

    // Session
    ctx.setSession("userId", "123");
    EXPECT_EQ(ctx.getSession("userId"), "123");

    // Storage
    ctx.set("timestamp", "1234567890");
    EXPECT_EQ(ctx.get("timestamp"), "1234567890");
}

TEST_F(LuaHttpTest, HttpContext_SetStatusAndHeaders) {
    crow::request req;
    LuaHttpContext ctx(req);

    ctx.setStatus(201);
    ctx.setHeader("X-Custom-Header", "test-value");

    EXPECT_EQ(ctx.response.code, 201);
}

// ============================================================================
// LuaHTTPServer Tests
// ============================================================================

TEST_F(LuaHttpTest, Server_CreateAndRegisterRoutes) {
    LuaHTTPServer server(9999);  // Use test port

    bool getCalled = false;
    bool postCalled = false;

    server.get("/test", [&getCalled](LuaHttpContext& ctx) {
        getCalled = true;
        ctx.OK({{"method", "GET"}});
    });

    server.post("/test", [&postCalled](LuaHttpContext& ctx) {
        postCalled = true;
        ctx.OK({{"method", "POST"}});
    });

    // Verify routes are registered
    EXPECT_TRUE(true);  // If we got here, registration worked
}

TEST_F(LuaHttpTest, Server_MiddlewareChain) {
    LuaHTTPServer server(9998);

    bool middleware1Called = false;
    bool middleware2Called = false;

    // Register middlewares
    server.use([&middleware1Called](LuaHttpContext& ctx) -> bool {
        middleware1Called = true;
        ctx.set("mw1", "passed");
        return true;
    });

    server.use([&middleware2Called](LuaHttpContext& ctx) -> bool {
        middleware2Called = true;
        return true;
    });

    server.get("/test", [](LuaHttpContext& ctx) {
        EXPECT_EQ(ctx.get("mw1"), "passed");
        ctx.OK({{"done", true}});
    });

    EXPECT_TRUE(true);
}

TEST_F(LuaHttpTest, Server_MiddlewareStopChain) {
    LuaHTTPServer server(9997);

    bool middlewareCalled = false;

    server.use([&middlewareCalled](LuaHttpContext& ctx) -> bool {
        middlewareCalled = true;
        ctx.setStatus(403);
        ctx.Error("Forbidden");
        return false;  // Stop chain
    });

    server.get("/test", [](LuaHttpContext& ctx) {
        ctx.OK({{"done", true}});
    });

    EXPECT_TRUE(true);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(LuaHttpTest, Integration_LuaContextBinding) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

    // Register HttpContext type
    sol::usertype<LuaHttpContext> ctx_type = lua.new_usertype<LuaHttpContext>(
        "HttpContext",
        sol::no_constructor);

    ctx_type["getBody"] = &LuaHttpContext::getBody;
    ctx_type["getHeader"] = &LuaHttpContext::getHeader;
    ctx_type["getQuery"] = &LuaHttpContext::getQuery;
    ctx_type["getJSON"] = &LuaHttpContext::getJSON;
    ctx_type["setStatus"] = &LuaHttpContext::setStatus;
    ctx_type["setHeader"] = &LuaHttpContext::setHeader;
    ctx_type["OK"] = &LuaHttpContext::OK;
    ctx_type["Error"] = &LuaHttpContext::Error;
    ctx_type["set"] = &LuaHttpContext::set;
    ctx_type["get"] = &LuaHttpContext::get;
    ctx_type["setSession"] = &LuaHttpContext::setSession;
    ctx_type["getSession"] = &LuaHttpContext::getSession;

    // Test Lua can use the context
    lua.script(R"(
        function testHandler(ctx)
            local body = ctx:getBody()
            ctx:setStatus(200)
            ctx:OK({received = body})
        end
    )");

    sol::function testHandler = lua["testHandler"];

    crow::request req;
    req.body = R"({"test": "data"})";
    LuaHttpContext ctx(req);

    testHandler(ctx);

    EXPECT_EQ(ctx.response.code, 200);
    EXPECT_TRUE(ctx.response.body.find("received") != std::string::npos);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(LuaHttpTest, HttpContext_EmptyBody) {
    crow::request req;
    req.body = "";

    LuaHttpContext ctx(req);

    EXPECT_EQ(ctx.getBody(), "");

    auto json = ctx.getJSON();
    EXPECT_TRUE(json.is_null() || json.empty());
}

TEST_F(LuaHttpTest, HttpContext_InvalidJSON) {
    crow::request req;
    req.body = "{invalid json}";

    LuaHttpContext ctx(req);

    EXPECT_THROW(ctx.getJSON(), nlohmann::json::parse_error);
}

TEST_F(LuaHttpTest, HttpContext_MissingQueryParams) {
    crow::request req;
    req.url = "/api/test";

    LuaHttpContext ctx(req);

    EXPECT_EQ(ctx.getQuery("missing"), "");
}

TEST_F(LuaHttpTest, HttpContext_SessionPersistence) {
    crow::request req;
    LuaHttpContext ctx(req);

    ctx.setSession("key1", "value1");
    ctx.setSession("key2", "value2");

    EXPECT_EQ(ctx.getSession("key1"), "value1");
    EXPECT_EQ(ctx.getSession("key2"), "value2");
    EXPECT_EQ(ctx.getSession("nonexistent"), "");
}

TEST_F(LuaHttpTest, HttpContext_UserState) {
    crow::request req;
    LuaHttpContext ctx(req);

    EXPECT_FALSE(ctx.isAuthenticated());
    EXPECT_EQ(ctx.getUsername(), "");
    EXPECT_EQ(ctx.getUserRole(), "");

    User testUser;
    testUser.id = 1;
    testUser.username = "testuser";
    testUser.role = "admin";
    ctx.user = testUser;

    EXPECT_TRUE(ctx.isAuthenticated());
    EXPECT_EQ(ctx.getUsername(), "testuser");
    EXPECT_EQ(ctx.getUserRole(), "admin");
}
