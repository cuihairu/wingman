// MCP 协议层与 tool 面单测（三段）：
// 1) handleMessage 协议矩阵：握手/版本协商/capabilities/错误码
// 2) run() 逐行帧环回：parse 错误/CRLF/空行/多帧 id 对应
// 3) callTool 工具面：注入 MockInput + 手写 MockUIA 元素树；script 腿走
//    script_manager_exec_coverage_test.cpp 同款接线（registerLuaEngine；
//    WINGMAN_HAS_LUA 来自 wingman 的 PUBLIC 定义，链接即见）。
// UIA 生产门面腿仅 Linux（Windows 会话 0 有真实 UIA 后端，不可断言）。
// 平台说明：除 Linux 生产腿外无平台分支，Windows CI 全量参与。

#include <gtest/gtest.h>

#include "wingman/mcp/mcp_server.hpp"
#include "wingman/mcp/mcp_tools.hpp"
#include "wingman/platform/mock_input.hpp"
#include "wingman/script_manager.hpp"
#include "wingman/ui_automation.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef WINGMAN_HAS_LUA
#include "wingman/lua/lua_script_engine.hpp"
#endif

using nlohmann::json;
using wingman::mcp::McpServer;
using wingman::mcp::ToolContext;
using wingman::mcp::callTool;

namespace {

// 手写 UIA mock：仓库只 mock IInput（编译进 lib），IUIAManager/IUIAElement
// 无现成 mock——此处按测试需要实现全部纯虚。
class MockUIAElement : public wingman::IUIAElement {
public:
    MockUIAElement(std::string name, std::string id, std::string className,
                   wingman::UIARole role)
        : info_([&] {
              wingman::UIAElementInfo i;
              i.name = std::move(name);
              i.id = std::move(id);
              i.className = std::move(className);
              i.role = role;
              i.bounds = wingman::Rect(10, 20, 100, 50);
              i.isEnabled = true;
              i.isVisible = true;
              return i;
          }()) {}

    std::string getName() const override { return info_.name; }
    std::string getId() const override { return info_.id; }
    std::string getClassName() const override { return info_.className; }
    wingman::UIARole getRole() const override { return info_.role; }
    wingman::Rect getBounds() const override { return info_.bounds; }
    bool isVisible() const override { return info_.isVisible; }
    bool isEnabled() const override { return info_.isEnabled; }
    std::string getText() const override { return info_.text; }
    bool setText(const std::string& text) override {
        info_.text = text;
        return true;
    }
    bool click() override {
        if (!clickOk_) return false;
        ++clicks_;
        return true;
    }
    bool setFocus() override {
        focused_ = true;
        return true;
    }
    bool hasFocus() const override { return focused_; }
    bool isChecked() const override { return checked_; }
    bool setChecked(bool checked) override {
        checked_ = checked;
        return true;
    }
    bool isValid() const override { return true; }
    wingman::UIAElementInfo getInfo() const override { return info_; }
    std::vector<std::shared_ptr<wingman::IUIAElement>> getChildren() override {
        return children_;
    }
    std::shared_ptr<wingman::IUIAElement> getParent() override { return nullptr; }
    bool select() override {
        selected_ = true;
        return true;
    }
    std::shared_ptr<wingman::IUIAElement> getSelection() override { return nullptr; }
    bool rightClick() override {
        ++rightClicks_;
        return true;
    }
    bool expand() override {
        expanded_ = true;
        return true;
    }
    bool collapse() override {
        expanded_ = false;
        return true;
    }
    bool isExpanded() const override { return expanded_; }
    bool doubleClick() override {
        ++doubleClicks_;
        return true;
    }

    void addChild(std::shared_ptr<MockUIAElement> child) {
        children_.push_back(std::move(child));
    }
    void setClickSucceeds(bool ok) { clickOk_ = ok; }

    int clicks_ = 0;
    int rightClicks_ = 0;
    int doubleClicks_ = 0;

private:
    wingman::UIAElementInfo info_;
    std::vector<std::shared_ptr<wingman::IUIAElement>> children_;
    bool focused_ = false;
    bool checked_ = false;
    bool selected_ = false;
    bool expanded_ = false;
    bool clickOk_ = true;
};

class ThrowingInfoElement : public MockUIAElement {
public:
    using MockUIAElement::MockUIAElement;
    wingman::UIAElementInfo getInfo() const override {
        throw std::runtime_error("backend died");
    }
};

class MockUIAManager : public wingman::IUIAManager {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    std::shared_ptr<wingman::IUIAElement> getFocusedElement() override { return nullptr; }
    std::shared_ptr<wingman::IUIAElement> getElementFromPoint(const wingman::Point&) override {
        return root_;
    }
    std::shared_ptr<wingman::IUIAElement> findElement(const wingman::UIASelector& selector) override {
        ++findCalls_;
        lastSelectorName_ = selector.name;
        return findIn(root_, selector);
    }
    std::shared_ptr<wingman::IUIAElement> getElementFromWindow(uint64_t) override { return root_; }
    std::vector<std::shared_ptr<wingman::IUIAElement>> findAllByRole(wingman::UIARole role) override {
        ++findAllCalls_;
        std::vector<std::shared_ptr<wingman::IUIAElement>> all;
        collect(root_, all);
        std::vector<std::shared_ptr<wingman::IUIAElement>> out;
        for (auto& e : all) {
            if (e->getRole() == role) out.push_back(e);
        }
        return out;
    }
    std::shared_ptr<wingman::IUIAElement> waitForElement(const wingman::UIASelector& selector,
                                                         int) override {
        return findIn(root_, selector);
    }
    uint64_t addPropertyChangedListener(const wingman::UIASelector&,
                                        wingman::IUIAManager::UIAEventCallback) override {
        return 1;
    }
    uint64_t addStructureChangedListener(const wingman::UIASelector&,
                                         wingman::IUIAManager::UIAEventCallback) override {
        return 2;
    }
    bool removeEventListener(uint64_t) override { return true; }
    std::string getBackendName() const override { return "mock"; }
    bool isAvailable() const override { return true; }

    // 树：root(Window) ─ ok(Button) ─ icon(Image) ─ deep(Unknown)
    //                └ cancel(Button) ─ text(TextBox)
    std::shared_ptr<MockUIAElement> root_ =
        std::make_shared<MockUIAElement>("Test Window", "win-1", "MainWindow",
                                         wingman::UIARole::Window);
    std::shared_ptr<MockUIAElement> ok_;
    std::shared_ptr<MockUIAElement> cancel_;
    int findCalls_ = 0;
    int findAllCalls_ = 0;
    std::string lastSelectorName_;

private:
    static std::shared_ptr<wingman::IUIAElement> findIn(
        const std::shared_ptr<wingman::IUIAElement>& node,
        const wingman::UIASelector& selector) {
        if (selector.matches(node->getInfo())) return node;
        for (const auto& child : node->getChildren()) {
            if (auto hit = findIn(child, selector)) return hit;
        }
        return nullptr;
    }
    static void collect(const std::shared_ptr<wingman::IUIAElement>& node,
                        std::vector<std::shared_ptr<wingman::IUIAElement>>& out) {
        out.push_back(node);
        for (const auto& child : node->getChildren()) collect(child, out);
    }
};

// CallToolResult → text 首块 JSON 解包（tool 面 payload 都在 content[0].text）。
json unwrapResult(const std::optional<json>& callResult) {
    EXPECT_TRUE(callResult.has_value());
    if (!callResult) return json::object();
    const auto& content = (*callResult).at("content");
    EXPECT_TRUE(content.is_array() && !content.empty());
    EXPECT_EQ(content[0].at("type").get<std::string>(), "text");
    return json::parse(content[0].at("text").get<std::string>());
}

json request(int64_t id, const std::string& method, json params = json::object()) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}};
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

class McpServerTest : public ::testing::Test {
protected:
    McpServer server_{"wingman", "9.9.9-test"};
};

class McpToolsTest : public ::testing::Test {
protected:
    void SetUp() override {
        ok_ = std::make_shared<MockUIAElement>("OK", "btn-ok", "Button", wingman::UIARole::Button);
        cancel_ = std::make_shared<MockUIAElement>("Cancel", "btn-cancel", "Button",
                                                   wingman::UIARole::Button);
        icon_ = std::make_shared<MockUIAElement>("OK Icon", "icon-1", "Image", wingman::UIARole::Image);
        deep_ = std::make_shared<MockUIAElement>("Deep", "deep-1", "Unknown",
                                                 wingman::UIARole::Unknown);
        text_ = std::make_shared<MockUIAElement>("Search", "tb-1", "TextBox",
                                                 wingman::UIARole::TextBox);
        ok_->addChild(icon_);
        icon_->addChild(deep_);
        cancel_->addChild(text_);
        manager_.root_->addChild(ok_);
        manager_.root_->addChild(cancel_);
        ctx_.uiaManager = &manager_;
    }

    MockUIAManager manager_;
    std::shared_ptr<MockUIAElement> ok_;
    std::shared_ptr<MockUIAElement> cancel_;
    std::shared_ptr<MockUIAElement> icon_;
    std::shared_ptr<MockUIAElement> deep_;
    std::shared_ptr<MockUIAElement> text_;
    ToolContext ctx_;
};

bool luaRegistered() {
    wingman::ScriptManager probe;
    const auto& languages = probe.getAvailableLanguages();
    return std::find(languages.begin(), languages.end(), "lua") != languages.end();
}

} // namespace

// ========== 协议矩阵 ==========

TEST_F(McpServerTest, InitializeEchoesSupportedVersion) {
    const json req = request(1, "initialize",
                             {{"protocolVersion", "2024-11-05"},
                              {"capabilities", json::object()},
                              {"clientInfo", {{"name", "tester"}, {"version", "0"}}}});
    const json resp = server_.handleMessage(req);

    EXPECT_EQ(resp.at("jsonrpc").get<std::string>(), "2.0");
    EXPECT_EQ(resp.at("id").get<int64_t>(), 1);
    const json& result = resp.at("result");
    EXPECT_EQ(result.at("protocolVersion").get<std::string>(), "2024-11-05");
    // capabilities 必带 tools 且必须是空对象：裸 {} 会被 nlohmann 序列化成
    // null，客户端读成未声明能力（冒烟实测抓过）
    ASSERT_TRUE(result.at("capabilities").at("tools").is_object());
    EXPECT_TRUE(result.at("capabilities").at("tools").empty());
    EXPECT_EQ(result.at("serverInfo").at("name").get<std::string>(), "wingman");
    EXPECT_EQ(result.at("serverInfo").at("version").get<std::string>(), "9.9.9-test");
}

TEST_F(McpServerTest, InitializeFallsBackToLatestOnUnknownVersion) {
    const json resp = server_.handleMessage(request(2, "initialize",
                                                    {{"protocolVersion", "1999-01-01"}}));
    EXPECT_EQ(resp.at("result").at("protocolVersion").get<std::string>(), "2025-11-25");
}

TEST_F(McpServerTest, InitializeWithoutParamsFallsBackToLatest) {
    const json resp = server_.handleMessage(request(3, "initialize"));
    EXPECT_EQ(resp.at("result").at("protocolVersion").get<std::string>(), "2025-11-25");
}

TEST_F(McpServerTest, ToolsListHasSevenTools) {
    const json resp = server_.handleMessage(request(4, "tools/list"));
    const json tools = resp.at("result").at("tools");

    ASSERT_TRUE(tools.is_array());
    ASSERT_EQ(tools.size(), 7u);
    std::vector<std::string> names;
    for (const auto& tool : tools) {
        names.push_back(tool.at("name").get<std::string>());
        EXPECT_EQ(tool.at("inputSchema").at("type").get<std::string>(), "object");
        EXPECT_FALSE(tool.at("description").get<std::string>().empty());
    }
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names,
              (std::vector<std::string>{"wingman_input_click", "wingman_screen_shot",
                                        "wingman_script_run", "wingman_uia_action",
                                        "wingman_uia_click", "wingman_uia_find",
                                        "wingman_uia_tree"}));
}

TEST_F(McpServerTest, ToolsListRequiredFields) {
    const json resp = server_.handleMessage(request(5, "tools/list"));
    const json tools = resp.at("result").at("tools");
    for (const auto& tool : tools) {
        const std::string name = tool.at("name").get<std::string>();
        if (name == "wingman_input_click") {
            const auto& required = tool.at("inputSchema").at("required");
            EXPECT_EQ(required, json::array({"x", "y"}));
        }
        if (name == "wingman_script_run") {
            const auto& required = tool.at("inputSchema").at("required");
            EXPECT_EQ(required, json::array({"code"}));
        }
        if (name == "wingman_uia_action") {
            const auto& required = tool.at("inputSchema").at("required");
            EXPECT_EQ(required, json::array({"handle", "action"}));
        }
    }
}

TEST_F(McpServerTest, PingReturnsEmptyResult) {
    const json resp = server_.handleMessage(request(6, "ping"));
    EXPECT_TRUE(resp.at("result").empty());
}

TEST_F(McpServerTest, UnknownMethodIs32601) {
    const json resp = server_.handleMessage(request(7, "resources/list"));
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32601);
}

TEST_F(McpServerTest, NotificationReturnsNullopt) {
    const json notification = {{"jsonrpc", "2.0"},
                               {"method", "notifications/initialized"}};
    EXPECT_EQ(server_.handleMessage(notification), std::nullopt);
}

TEST_F(McpServerTest, NonObjectRequestIs32600) {
    const json resp = server_.handleMessage(json::array({1, 2}));
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32600);
}

TEST_F(McpServerTest, MissingMethodIs32600) {
    const json resp = server_.handleMessage({{"jsonrpc", "2.0"}, {"id", 8}});
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32600);
}

TEST_F(McpServerTest, UnknownToolIs32602) {
    const json resp = server_.handleMessage(
        request(9, "tools/call", {{"name", "wingman_nope"}, {"arguments", json::object()}}));
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32602);
    EXPECT_NE(resp.at("error").at("message").get<std::string>().find("Unknown tool"),
              std::string::npos);
}

TEST_F(McpServerTest, ToolsCallWithoutNameIs32602) {
    const json resp =
        server_.handleMessage(request(10, "tools/call", {{"arguments", json::object()}}));
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32602);
}

TEST_F(McpServerTest, ToolsCallWithNonObjectArgumentsIs32602) {
    const json resp = server_.handleMessage(
        request(11, "tools/call", {{"name", "wingman_ping"}, {"arguments", json::array({1})}}));
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32602);
}

// ========== run() 逐行帧环回 ==========

TEST_F(McpServerTest, ParseErrorReturns32700WithNullId) {
    std::istringstream in("this is not json\n");
    std::ostringstream out;
    server_.run(in, out);

    const auto lines = splitLines(out.str());
    ASSERT_EQ(lines.size(), 1u);
    const json resp = json::parse(lines[0]);
    EXPECT_EQ(resp.at("error").at("code").get<int>(), -32700);
    EXPECT_TRUE(resp.at("id").is_null());
}

TEST_F(McpServerTest, CrlfFramesAreAccepted) {
    std::istringstream in("{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"ping\"}\r\n");
    std::ostringstream out;
    server_.run(in, out);

    const auto lines = splitLines(out.str());
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(json::parse(lines[0]).at("id").get<int>(), 12);
}

TEST_F(McpServerTest, EmptyLinesProduceNoOutput) {
    std::istringstream in("\n\n\n");
    std::ostringstream out;
    server_.run(in, out);
    EXPECT_TRUE(out.str().empty());
}

TEST_F(McpServerTest, MultipleFramesRespondLineByLineAndSkipNotifications) {
    std::ostringstream frames;
    frames << request(13, "ping").dump() << '\n';
    frames << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    frames << request(14, "tools/list").dump() << '\n';

    std::istringstream in(frames.str());
    std::ostringstream out;
    server_.run(in, out);

    const auto lines = splitLines(out.str());
    ASSERT_EQ(lines.size(), 2u); // notification 不产生帧
    EXPECT_EQ(json::parse(lines[0]).at("id").get<int>(), 13);
    const json tools = json::parse(lines[1]).at("result").at("tools");
    EXPECT_EQ(tools.size(), 7u);
}

TEST_F(McpServerTest, ToolFailureInRunLoopIsErrorResultNotProtocolError) {
    // tool 内失败 = result.isError（LLM 可见）；找不到 tool 才是协议 error
    std::istringstream in(request(15, "tools/call",
                                 {{"name", "wingman_script_run"},
                                  {"arguments", json::object()}})
                              .dump()
                              .append("\n"));
    std::ostringstream out;
    server_.run(in, out);

    const auto lines = splitLines(out.str());
    ASSERT_EQ(lines.size(), 1u);
    const json resp = json::parse(lines[0]);
    EXPECT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp.at("result").at("isError").get<bool>());
}

// ========== 工具面：uia_tree / uia_find ==========

TEST_F(McpToolsTest, UiaTreeReturnsNestedTreeWithHandles) {
    const json payload = unwrapResult(callTool("wingman_uia_tree", json::object(), ctx_));

    const json& root = payload.at("elements");
    EXPECT_EQ(root.at("name").get<std::string>(), "Test Window");
    EXPECT_EQ(root.at("role").get<std::string>(), "Window");
    EXPECT_GT(root.at("handle").get<int>(), 0);
    EXPECT_TRUE(root.at("bounds").contains("width"));
    EXPECT_TRUE(root.contains("children"));

    // 层级 2：root 的 children（depth 2）
    const json& level2 = root.at("children");
    ASSERT_EQ(level2.size(), 2u);
    EXPECT_EQ(level2[0].at("name").get<std::string>(), "OK");
    EXPECT_EQ(level2[0].at("role").get<std::string>(), "Button");

    // 层级 3：grandchild（depth 1）仍带 children 数组
    const json& grandchild = level2[0].at("children");
    ASSERT_EQ(grandchild.size(), 1u);
    EXPECT_EQ(grandchild[0].at("name").get<std::string>(), "OK Icon");

    // 层级 4：深度上限截断（depth 0 不再展开 children 键）
    const json& greatGrandchild = grandchild[0].at("children");
    ASSERT_EQ(greatGrandchild.size(), 1u);
    EXPECT_EQ(greatGrandchild[0].at("name").get<std::string>(), "Deep");
    EXPECT_FALSE(greatGrandchild[0].contains("children"));
}

TEST_F(McpToolsTest, UiaTreeNullRootIsEmptyResultNotError) {
    manager_.root_ = nullptr;
    const json payload = unwrapResult(callTool("wingman_uia_tree", json::object(), ctx_));
    EXPECT_TRUE(payload.at("elements").is_array());
    EXPECT_TRUE(payload.at("elements").empty());
    EXPECT_FALSE(payload.contains("error"));
}

TEST_F(McpToolsTest, UiaTreeWindowNotFoundIsError) {
    // Window::find 对不存在的标题在两平台都返回 0 → 明确报错不静默换前台
    const json payload =
        unwrapResult(callTool("wingman_uia_tree", {{"window", "no-such-window-98765"}}, ctx_));
    EXPECT_TRUE(payload.at("error").get<std::string>().find("window not found") !=
                std::string::npos);
}

TEST_F(McpToolsTest, UiaFindByRoleUsesMultiQueryAndFilters) {
    const json all = unwrapResult(callTool("wingman_uia_find", {{"role", "Button"}}, ctx_));
    EXPECT_EQ(manager_.findAllCalls_, 1);
    ASSERT_EQ(all.at("elements").size(), 2u); // OK + Cancel

    const json filtered = unwrapResult(
        callTool("wingman_uia_find", {{"role", "Button"}, {"name", "Cancel"}}, ctx_));
    ASSERT_EQ(filtered.at("elements").size(), 1u);
    EXPECT_EQ(filtered.at("elements")[0].at("name").get<std::string>(), "Cancel");
    EXPECT_GT(filtered.at("elements")[0].at("handle").get<int>(), 0);
}

TEST_F(McpToolsTest, UiaFindByNameUsesSingleQuery) {
    const json payload =
        unwrapResult(callTool("wingman_uia_find", {{"name", "Search"}}, ctx_));
    EXPECT_EQ(manager_.findCalls_, 1);
    EXPECT_EQ(manager_.lastSelectorName_, "Search");
    ASSERT_EQ(payload.at("elements").size(), 1u);
    EXPECT_EQ(payload.at("elements")[0].at("id").get<std::string>(), "tb-1");
}

TEST_F(McpToolsTest, UiaFindNoMatchIsEmptyNotError) {
    const json payload = unwrapResult(callTool("wingman_uia_find", {{"role", "Slider"}}, ctx_));
    EXPECT_TRUE(payload.at("elements").empty());
    EXPECT_FALSE(payload.contains("error"));
}

TEST_F(McpToolsTest, UiaFindEmptySelectorRejected) {
    const json payload = unwrapResult(callTool("wingman_uia_find", json::object(), ctx_));
    EXPECT_TRUE(payload.at("error").get<std::string>().find("at least one criterion") !=
                std::string::npos);
}

TEST_F(McpToolsTest, UiaFindUnknownRoleRejected) {
    const json payload = unwrapResult(callTool("wingman_uia_find", {{"role", "Bogus"}}, ctx_));
    EXPECT_TRUE(payload.at("error").get<std::string>().find("unknown role") != std::string::npos);
}

// ========== 工具面：uia_click / uia_action ==========

TEST_F(McpToolsTest, UiaClickByHandleFromFind) {
    const json found = unwrapResult(callTool("wingman_uia_find", {{"name", "OK"}}, ctx_));
    const int handle = found.at("elements")[0].at("handle").get<int>();

    const json payload = unwrapResult(callTool("wingman_uia_click", {{"handle", handle}}, ctx_));
    EXPECT_EQ(payload.at("clicked").get<bool>(), true);
    EXPECT_EQ(ok_->clicks_, 1);
}

TEST_F(McpToolsTest, UiaClickByName) {
    const json payload = unwrapResult(callTool("wingman_uia_click", {{"name", "Cancel"}}, ctx_));
    EXPECT_EQ(payload.at("clicked").get<bool>(), true);
    EXPECT_EQ(cancel_->clicks_, 1);
}

TEST_F(McpToolsTest, UiaClickFailuresStayExplicit) {
    // 失败不转坐标：click() 假返回明确 isError
    ok_->setClickSucceeds(false);
    const json found = unwrapResult(callTool("wingman_uia_find", {{"name", "OK"}}, ctx_));
    const int handle = found.at("elements")[0].at("handle").get<int>();
    const json failed = unwrapResult(callTool("wingman_uia_click", {{"handle", handle}}, ctx_));
    EXPECT_TRUE(failed.at("error").get<std::string>().find("click failed") != std::string::npos);

    // 无定位标识
    const json noId = unwrapResult(callTool("wingman_uia_click", json::object(), ctx_));
    EXPECT_TRUE(noId.contains("error"));

    // 坏句柄
    const json badHandle =
        unwrapResult(callTool("wingman_uia_click", {{"handle", 987654321}}, ctx_));
    EXPECT_TRUE(badHandle.at("error").get<std::string>().find("invalid element handle") !=
                std::string::npos);
}

TEST_F(McpToolsTest, UiaActionAllNineActions) {
    const auto handleOf = [&](const std::string& name) {
        const json found = unwrapResult(callTool("wingman_uia_find", {{"name", name}}, ctx_));
        return found.at("elements")[0].at("handle").get<int>();
    };
    const auto actionOk = [&](json args) {
        return unwrapResult(callTool("wingman_uia_action", args, ctx_)).at("ok").get<bool>();
    };
    const int okHandle = handleOf("OK");
    const int textHandle = handleOf("Search");

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "click"}}));
    EXPECT_EQ(ok_->clicks_, 1);

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "doubleClick"}}));
    EXPECT_EQ(ok_->doubleClicks_, 1);

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "rightClick"}}));
    EXPECT_EQ(ok_->rightClicks_, 1);

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "setFocus"}}));
    EXPECT_TRUE(ok_->hasFocus());

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "select"}}));

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "expand"}}));
    EXPECT_TRUE(ok_->isExpanded());

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "collapse"}}));
    EXPECT_FALSE(ok_->isExpanded());

    EXPECT_TRUE(actionOk({{"handle", textHandle}, {"action", "setText"}, {"value", "new text"}}));
    EXPECT_EQ(text_->getText(), "new text");

    EXPECT_TRUE(actionOk({{"handle", okHandle}, {"action", "setChecked"}, {"checked", true}}));
    EXPECT_TRUE(ok_->isChecked());
}

TEST_F(McpToolsTest, UiaActionValidationMatrix) {
    const auto handleOf = [&](const std::string& name) {
        const json found = unwrapResult(callTool("wingman_uia_find", {{"name", name}}, ctx_));
        return found.at("elements")[0].at("handle").get<int>();
    };
    const auto actionPayload = [&](json args) {
        return unwrapResult(callTool("wingman_uia_action", args, ctx_));
    };
    const int okHandle = handleOf("OK");

    // 缺 handle / 缺 action / 坏 handle / 未知 action
    EXPECT_TRUE(actionPayload({{"action", "click"}}).contains("error"));
    EXPECT_TRUE(actionPayload({{"handle", okHandle}}).contains("error"));
    EXPECT_NE(actionPayload({{"handle", 12321}, {"action", "click"}})
                  .at("error")
                  .get<std::string>()
                  .find("invalid element handle"),
              std::string::npos);
    EXPECT_NE(actionPayload({{"handle", okHandle}, {"action", "smash"}})
                  .at("error")
                  .get<std::string>()
                  .find("unknown action"),
              std::string::npos);
}

// ========== 工具面：input_click（MockInput 注入） ==========

TEST_F(McpToolsTest, InputClickMovesThenClicks) {
    wingman::platform::mock::MockInput input;
    ctx_.input = &input;

    const json payload = unwrapResult(callTool("wingman_input_click",
                                               {{"x", 120}, {"y", 80}}, ctx_));
    EXPECT_EQ(payload.at("clicked").get<bool>(), true);
    EXPECT_EQ(payload.at("button").get<std::string>(), "left");
    EXPECT_TRUE(input.isInitialized());
    EXPECT_EQ(input.getMouseMoveCallCount(), 1);
    EXPECT_EQ(input.getClickCallCount(wingman::platform::MouseButton::Left), 1);

    const json right = unwrapResult(callTool("wingman_input_click",
                                             {{"x", 1}, {"y", 2}, {"button", "right"}}, ctx_));
    EXPECT_EQ(right.at("button").get<std::string>(), "right");
    EXPECT_EQ(input.getClickCallCount(wingman::platform::MouseButton::Right), 1);
}

TEST_F(McpToolsTest, InputClickValidationMatrix) {
    wingman::platform::mock::MockInput input;
    ctx_.input = &input;

    const auto clickPayload = [&](json args) {
        return unwrapResult(callTool("wingman_input_click", args, ctx_));
    };
    EXPECT_TRUE(clickPayload({{"x", 1}}).contains("error"));
    EXPECT_TRUE(clickPayload({{"x", 1}, {"y", 1.5}}).contains("error"));
    EXPECT_TRUE(clickPayload({{"x", 1}, {"y", 1}, {"button", "middle"}}).contains("error"));
    EXPECT_EQ(input.getClickCallCount(wingman::platform::MouseButton::Left), 0);
}

// ========== 工具面：screen_shot（vision 编译分支） ==========

TEST_F(McpToolsTest, ScreenShotWithoutVisionBuildIsExplicitError) {
#ifdef WINGMAN_ENABLE_VISION
    GTEST_SKIP() << "vision build: degradation leg not compiled";
#else
    const json payload =
        unwrapResult(callTool("wingman_screen_shot", json::object(), ToolContext{}));
    EXPECT_TRUE(payload.at("error").get<std::string>().find("vision build") !=
                std::string::npos);
#endif
}

#ifdef WINGMAN_ENABLE_VISION
TEST_F(McpToolsTest, ScreenShotRegionValidation) {
    const json bad = unwrapResult(
        callTool("wingman_screen_shot", {{"region", {0, 0, -5, 10}}}, ToolContext{}));
    EXPECT_TRUE(bad.at("error").get<std::string>().find("positive") != std::string::npos);
}
#endif

// ========== 工具面：script_run（真实 Lua 引擎） ==========

class McpScriptTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef WINGMAN_HAS_LUA
        // 幂等（内部 static registrar），与 runtime main 的调用等价
        wingman::lua::registerLuaEngine();
#endif
    }

    // script tool 不注入（生产路径：ctx.scriptManager 为空 → 局部 manager）
    ToolContext ctx_;
};

TEST_F(McpScriptTest, ScriptRunPrintCapturesOutput) {
#ifdef WINGMAN_HAS_LUA
    if (!luaRegistered()) GTEST_SKIP() << "lua engine not available";
    const json payload = unwrapResult(callTool("wingman_script_run",
                                               {{"code", "print(\"mcp-hello\")"}}, ctx_));
    EXPECT_EQ(payload.at("ok").get<bool>(), true);
    EXPECT_EQ(payload.at("output").get<std::string>(), "mcp-hello\n");
#else
    GTEST_SKIP() << "lua not available";
#endif
}

TEST_F(McpScriptTest, ScriptRunValidationMatrix) {
#ifdef WINGMAN_HAS_LUA
    if (!luaRegistered()) GTEST_SKIP() << "lua engine not available";

    const auto runPayload = [&](json args) {
        return unwrapResult(callTool("wingman_script_run", args, ctx_));
    };
    EXPECT_TRUE(runPayload(json::object()).contains("error"));
    EXPECT_TRUE(runPayload({{"code", 123}}).contains("error"));
    EXPECT_TRUE(runPayload({{"code", ""}}).contains("error"));
    EXPECT_TRUE(runPayload({{"code", "return 1"}, {"timeoutMs", 0}}).contains("error"));
    EXPECT_TRUE(runPayload({{"code", "return 1"}, {"timeoutMs", "x"}}).contains("error"));
#else
    GTEST_SKIP() << "lua not available";
#endif
}

TEST_F(McpScriptTest, ScriptRunTimeoutReportsErrorAndAbandons) {
#ifdef WINGMAN_HAS_LUA
    if (!luaRegistered()) GTEST_SKIP() << "lua engine not available";
    // 有限长循环（同 script_manager_exec_coverage_test 先例：detached 线程
    // 自然跑完，不用 while true 忙等拖累全量）
    const std::string code =
        "local n = 0\nfor i = 1, 500000000 do n = n + i end\nreturn n\n";
    const json payload = unwrapResult(callTool("wingman_script_run",
                                               {{"code", code}, {"timeoutMs", 300}}, ctx_));
    EXPECT_EQ(payload.at("ok").get<bool>(), false);
    EXPECT_NE(payload.at("error").get<std::string>().find("timeout"), std::string::npos);
#else
    GTEST_SKIP() << "lua not available";
#endif
}

// ========== role 名往返全枚举 / tool crash 兜底 ==========

TEST_F(McpToolsTest, UiaFindCyclesAllKnownRoles) {
    // roleFromString 全表轮询：每个合法 role 名都被接受（不报 unknown role）。
    // "Unknown" 除外——工具把 Unknown 当哨兵（无意义查询），显式请求即报错。
    static const std::vector<const char*> kRoleNames = {
        "Window", "Button", "TextBox", "CheckBox", "RadioButton", "ComboBox",
        "ListBox", "ListItem", "Menu", "MenuItem", "Table", "Tree", "TreeItem",
        "Hyperlink", "Image", "Slider", "Spinner", "ProgressBar", "Tab",
        "TabItem"};
    for (const char* roleName : kRoleNames) {
        const json payload =
            unwrapResult(callTool("wingman_uia_find", {{"role", roleName}}, ctx_));
        EXPECT_FALSE(payload.contains("error")) << roleName;
        EXPECT_TRUE(payload.at("elements").is_array()) << roleName;
    }
}

TEST_F(McpToolsTest, UiaTreeEmitsAllRoleNames) {
    // roleToString 全枚举：21 个 role 的元素经树输出后名字齐全（LLM 消费面
    // 只见名字不见整数枚举——键口径契约）
    static const std::vector<std::pair<std::string, wingman::UIARole>> kAllRoles = {
        {"CheckBox", wingman::UIARole::CheckBox},
        {"RadioButton", wingman::UIARole::RadioButton},
        {"ComboBox", wingman::UIARole::ComboBox},
        {"ListBox", wingman::UIARole::ListBox},
        {"ListItem", wingman::UIARole::ListItem},
        {"Menu", wingman::UIARole::Menu},
        {"MenuItem", wingman::UIARole::MenuItem},
        {"Table", wingman::UIARole::Table},
        {"Tree", wingman::UIARole::Tree},
        {"TreeItem", wingman::UIARole::TreeItem},
        {"Hyperlink", wingman::UIARole::Hyperlink},
        {"Slider", wingman::UIARole::Slider},
        {"Spinner", wingman::UIARole::Spinner},
        {"ProgressBar", wingman::UIARole::ProgressBar},
        {"Tab", wingman::UIARole::Tab},
        {"TabItem", wingman::UIARole::TabItem},
    };
    for (const auto& [name, role] : kAllRoles) {
        manager_.root_->addChild(
            std::make_shared<MockUIAElement>(name, "id-" + name, "Cls", role));
    }
    const json payload = unwrapResult(callTool("wingman_uia_tree", json::object(), ctx_));

    std::vector<std::string> roles;
    for (const auto& child : payload.at("elements").at("children")) {
        roles.push_back(child.at("role").get<std::string>());
    }
    // fixture 树原有 OK/Cancel 两个 Button + 16 个新叶子
    ASSERT_EQ(roles.size(), kAllRoles.size() + 2u);
    std::sort(roles.begin(), roles.end());
    std::vector<std::string> expected;
    for (const auto& [name, role] : kAllRoles) {
        expected.push_back(name);
    }
    expected.push_back("Button");
    expected.push_back("Button");
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(roles, expected);
}

TEST_F(McpToolsTest, ToolCrashBecomesErrorResult) {
    // tool 内异常由协议层（handleToolsCall）兜成 isError（"tool crashed"），
    // 进程不炸、协议流不断；callTool 直调不设防，异常按契约传播。
    // crash 注入经 McpServer 的 ToolContext 构造缝（默认走生产单例）。
    manager_.root_->addChild(std::make_shared<ThrowingInfoElement>(
        "Crashy", "c-1", "Cls", wingman::UIARole::Button));
    McpServer server{"wingman", "9.9.9-test", ctx_};
    const json resp = server.handleMessage(
        request(21, "tools/call",
                {{"name", "wingman_uia_click"}, {"arguments", {{"name", "Crashy"}}}}));
    const json payload = json::parse(resp.at("result")
                                         .at("content")[0]
                                         .at("text")
                                         .get<std::string>());
    EXPECT_TRUE(resp.at("result").at("isError").get<bool>());
    EXPECT_NE(payload.at("error").get<std::string>().find("tool crashed"),
              std::string::npos);
    EXPECT_NE(payload.at("error").get<std::string>().find("backend died"),
              std::string::npos);
}

// ========== 生产门面腿（仅 Linux：无 UIA 后端 → 明确 isError） ==========

#ifndef _WIN32
TEST(McpProductionFacadeTest, UiaToolsWithoutBackendReportUnsupported) {
    ToolContext ctx; // 全 nullptr = 生产单例（Linux 工厂 nullptr → initialize false）
    const json payload = unwrapResult(callTool("wingman_uia_tree", json::object(), ctx));
    EXPECT_TRUE(payload.at("error").get<std::string>().find("not supported") !=
                std::string::npos);

    const json find = unwrapResult(callTool("wingman_uia_find", {{"name", "x"}}, ctx));
    EXPECT_TRUE(find.contains("error"));
}
#endif
