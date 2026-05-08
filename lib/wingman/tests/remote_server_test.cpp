#include <gtest/gtest.h>
#include "wingman/remote_server.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

class RemoteServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server = std::make_unique<RemoteServer>();
        client = std::make_unique<RemoteClient>();
    }

    void TearDown() override {
        if (client && client->isConnected()) {
            client->disconnect();
        }
        if (server && server->isRunning()) {
            server->stop();
        }
        server.reset();
        client.reset();
    }

    std::unique_ptr<RemoteServer> server;
    std::unique_ptr<RemoteClient> client;
};

// ========== RemoteRequest/Response ==========

TEST_F(RemoteServerTest, RemoteRequestFromJson) {
    nlohmann::json j;
    j["action"] = "ping";
    j["params"] = nlohmann::json::object();

    RemoteRequest req = RemoteRequest::fromJson(j);
    EXPECT_EQ(req.action, "ping");
    EXPECT_TRUE(req.params.is_null() || req.params.is_object());
}

TEST_F(RemoteServerTest, RemoteRequestToJson) {
    RemoteRequest req;
    req.action = "click";
    req.params = {{"x", 100}, {"y", 200}};

    nlohmann::json j = req.toJson();
    EXPECT_EQ(j["action"], "click");
    EXPECT_EQ(j["params"]["x"], 100);
    EXPECT_EQ(j["params"]["y"], 200);
}

TEST_F(RemoteServerTest, RemoteResponseToJsonString) {
    RemoteResponse resp;
    resp.success = true;
    resp.data = {{"result", "ok"}};

    std::string jsonStr = resp.toJsonString();
    EXPECT_FALSE(jsonStr.empty());

    nlohmann::json j = nlohmann::json::parse(jsonStr);
    EXPECT_TRUE(j["success"]);
    EXPECT_EQ(j["data"]["result"], "ok");
}

// ========== RemoteServer ==========

TEST_F(RemoteServerTest, ServerInitialState) {
    EXPECT_FALSE(server->isRunning());
    EXPECT_EQ(server->getConnectionCount(), 0);
}

TEST_F(RemoteServerTest, ServerStartStop) {
    EXPECT_TRUE(server->start(9999));
    EXPECT_TRUE(server->isRunning());
    EXPECT_EQ(server->getPort(), 9999);

    // 重复start应该是安全的
    EXPECT_TRUE(server->start(9999));

    server->stop();
    EXPECT_FALSE(server->isRunning());
}

// ========== RemoteClient ==========

TEST_F(RemoteServerTest, ClientInitialState) {
    EXPECT_FALSE(client->isConnected());
}

TEST_F(RemoteServerTest, ClientConnectDisconnect) {
    // 启动服务器
    ASSERT_TRUE(server->start(9998));

    // 稍等服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 连接
    ASSERT_TRUE(client->connect("127.0.0.1", 9998));
    EXPECT_TRUE(client->isConnected());

    // 断开
    client->disconnect();
    EXPECT_FALSE(client->isConnected());
}

TEST_F(RemoteServerTest, ClientPing) {
    ASSERT_TRUE(server->start(9997));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9997));

    RemoteResponse resp = client->ping();
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.data["status"], "ok");

    client->disconnect();
}

TEST_F(RemoteServerTest, ClientGetVersion) {
    ASSERT_TRUE(server->start(9996));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9996));

    RemoteRequest req;
    req.action = "get_version";
    RemoteResponse resp = client->send(req);

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.data["version"], "0.2.0");
    EXPECT_EQ(resp.data["name"], "Wingman");

    client->disconnect();
}

TEST_F(RemoteServerTest, ClientSend) {
    ASSERT_TRUE(server->start(9995));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9995));

    // 测试send方法
    RemoteResponse resp = client->send("ping");
    EXPECT_TRUE(resp.success);

    // 测试带参数的send
    resp = client->send("get_version", {{"extra", "param"}});
    EXPECT_TRUE(resp.success);

    client->disconnect();
}

TEST_F(RemoteServerTest, ClientGetPixel) {
    ASSERT_TRUE(server->start(9994));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9994));

    RemoteResponse resp = client->getPixel(100, 100);
    EXPECT_TRUE(resp.success);
    EXPECT_TRUE(resp.data.contains("r"));
    EXPECT_TRUE(resp.data.contains("g"));
    EXPECT_TRUE(resp.data.contains("b"));

    client->disconnect();
}

TEST_F(RemoteServerTest, ClientListTriggers) {
    ASSERT_TRUE(server->start(9993));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9993));

    RemoteResponse resp = client->listTriggers();
    EXPECT_TRUE(resp.success);
    EXPECT_TRUE(resp.data.contains("count"));
    EXPECT_TRUE(resp.data.contains("triggers"));

    client->disconnect();
}

// ========== 错误处理 ==========

TEST_F(RemoteServerTest, ClientConnectToNonExistentServer) {
    EXPECT_FALSE(client->connect("127.0.0.1", 9990));
    EXPECT_FALSE(client->isConnected());
}

TEST_F(RemoteServerTest, UnknownAction) {
    ASSERT_TRUE(server->start(9992));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9992));

    RemoteResponse resp = client->send("unknown_action");
    EXPECT_FALSE(resp.success);
    EXPECT_FALSE(resp.error.empty());

    client->disconnect();
}
