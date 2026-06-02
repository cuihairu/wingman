#include <gtest/gtest.h>
#include "wingman/remote_server.hpp"
#include "wingman/version.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

class RemoteControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        server = std::make_unique<RemoteControlServer>();
        client = std::make_unique<RemoteControlClient>();
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

    std::unique_ptr<RemoteControlServer> server;
    std::unique_ptr<RemoteControlClient> client;
};

// ========== RemoteRequest/Response ==========

TEST_F(RemoteControlTest, RemoteRequestFromJson) {
    nlohmann::json j;
    j["action"] = "ping";
    j["params"] = nlohmann::json::object();

    RemoteRequest req = RemoteRequest::fromJson(j);
    EXPECT_EQ(req.action, "ping");
    EXPECT_TRUE(req.params.is_null() || req.params.is_object());
}

TEST_F(RemoteControlTest, RemoteRequestToJson) {
    RemoteRequest req;
    req.action = "click";
    req.params = {{"x", 100}, {"y", 200}};

    nlohmann::json j = req.toJson();
    EXPECT_EQ(j["action"], "click");
    EXPECT_EQ(j["params"]["x"], 100);
    EXPECT_EQ(j["params"]["y"], 200);
}

TEST_F(RemoteControlTest, RemoteResponseToJsonString) {
    RemoteResponse resp;
    resp.success = true;
    resp.data = {{"result", "ok"}};

    std::string jsonStr = resp.toJsonString();
    EXPECT_FALSE(jsonStr.empty());

    nlohmann::json j = nlohmann::json::parse(jsonStr);
    EXPECT_TRUE(j["success"]);
    EXPECT_EQ(j["data"]["result"], "ok");
}

// ========== RemoteControlServer ==========

TEST_F(RemoteControlTest, ServerInitialState) {
    EXPECT_FALSE(server->isRunning());
    EXPECT_EQ(server->getConnectionCount(), 0);
}

TEST_F(RemoteControlTest, ServerStartStop) {
    EXPECT_TRUE(server->start(9999));
    EXPECT_TRUE(server->isRunning());
    EXPECT_EQ(server->getPort(), 9999);

    // Repeated start should be safe
    EXPECT_TRUE(server->start(9999));

    server->stop();
    EXPECT_FALSE(server->isRunning());
}

// ========== RemoteControlClient ==========

TEST_F(RemoteControlTest, ClientInitialState) {
    EXPECT_FALSE(client->isConnected());
}

TEST_F(RemoteControlTest, ClientConnectDisconnect) {
    // Start server
    ASSERT_TRUE(server->start(9998));

    // Wait briefly for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect
    ASSERT_TRUE(client->connect("127.0.0.1", 9998));
    EXPECT_TRUE(client->isConnected());

    // Send a request to ensure the server-side thread works properly
    RemoteResponse resp = client->ping();
    EXPECT_TRUE(resp.success);

    // Wait briefly for server to finish processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Disconnect
    client->disconnect();
    EXPECT_FALSE(client->isConnected());

    // Wait briefly for server-side thread to exit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(RemoteControlTest, ClientPing) {
    ASSERT_TRUE(server->start(9997));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9997));

    RemoteResponse resp = client->ping();
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.data["status"], "ok");

    client->disconnect();
}

TEST_F(RemoteControlTest, ClientGetVersion) {
    ASSERT_TRUE(server->start(9996));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9996));

    RemoteRequest req;
    req.action = "get_version";
    RemoteResponse resp = client->send(req);

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.data["version"], std::string(WINGMAN_VERSION));
    EXPECT_EQ(resp.data["name"], "Wingman");

    client->disconnect();
}

TEST_F(RemoteControlTest, ClientSend) {
    ASSERT_TRUE(server->start(9995));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9995));

    // Test send method
    RemoteResponse resp = client->send("ping");
    EXPECT_TRUE(resp.success);

    // Test send with parameters
    resp = client->send("get_version", {{"extra", "param"}});
    EXPECT_TRUE(resp.success);

    client->disconnect();
}

TEST_F(RemoteControlTest, ClientGetPixel) {
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

TEST_F(RemoteControlTest, ClientListTriggers) {
    // Use port 9988 to avoid conflict with other tests
    ASSERT_TRUE(server->start(9988));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ASSERT_TRUE(client->connect("127.0.0.1", 9988));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    RemoteResponse resp = client->listTriggers();
    EXPECT_TRUE(resp.success) << "Error: " << resp.error;
    if (resp.success) {
        EXPECT_TRUE(resp.data.contains("count"));
        EXPECT_TRUE(resp.data.contains("triggers"));
    }

    // Wait briefly for server to finish processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client->disconnect();
}

// ========== Error Handling ==========

TEST_F(RemoteControlTest, ClientConnectToNonExistentServer) {
    EXPECT_FALSE(client->connect("127.0.0.1", 9990));
    EXPECT_FALSE(client->isConnected());
}

TEST_F(RemoteControlTest, UnknownAction) {
    ASSERT_TRUE(server->start(9992));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(client->connect("127.0.0.1", 9992));

    RemoteResponse resp = client->send("unknown_action");
    EXPECT_FALSE(resp.success);
    EXPECT_FALSE(resp.error.empty());

    client->disconnect();
}
