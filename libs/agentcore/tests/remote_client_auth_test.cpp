#include "agentcore_test_common.hpp"

// ========== challenge-response（A3-P2 §6.1，hmac_sha256 + RemoteClient） ==========

TEST(HmacSha256Test, Sha256KnownVectors) {
    // FIPS 180 标准向量
    EXPECT_EQ(sha256Hex(""),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256Hex("abc"),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // 跨块长度（> 64 字节）验证多块压缩与填充
    EXPECT_EQ(sha256Hex(std::string(200, 'x')),
        "aa20c23e3201834050679e1d88941b9a6fed0557c9a705cb2c315e2e63fd486d");
}

TEST(HmacSha256Test, HmacRfc4231Vectors) {
    // RFC 4231 Test Case 1/2（key/data 原始字节；本层返回原始 32 字节，经
    // 内联 hex 参考实现转 hex 后比对）
    EXPECT_EQ(testHexEncode(hmacSha256(std::string(20, '\x0b'), "Hi There")),
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    EXPECT_EQ(testHexEncode(hmacSha256("Jefe", "what do ya want for nothing?")),
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    // key 超过块长（64 字节）走 key 先哈希分支（RFC 4231 Test Case 7）
    EXPECT_EQ(testHexEncode(hmacSha256(std::string(131, '\xaa'),
                      "Test Using Larger Than Block-Size Key - Hash Key First")),
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

TEST(HmacSha256Test, HmacHexKeyMatchesServerTokenMac) {
    // 与 Go server tokenMAC 字节语义对齐：key/message 均为 hex 串的 ASCII 字节。
    // 向量独立预计算：key = sha256Hex("tok-1")，message = nonce。
    const std::string key = "65dcf16ea3dfa49069628089eb4a75483070f5584b2a21ee64912b5f621f12da";
    const std::string nonce = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
    EXPECT_EQ(hmacSha256HexKey(key, nonce),
        "aa1b9280bd896237986b9cc680446c0833f6cedeaffe1b2e47b0bcd337f1f4d6");
    // 与「先 sha256Hex(token) 再 HMAC」的组合用法一致
    EXPECT_EQ(hmacSha256HexKey(sha256Hex("tok-1"), nonce),
        "aa1b9280bd896237986b9cc680446c0833f6cedeaffe1b2e47b0bcd337f1f4d6");
}

TEST_F(RemoteClientTest, RegisterChallengeModeCarriesFlagNotToken) {
    ASSERT_TRUE(startServer());
    auto config = makeConfig();
    config.useChallengeAuth = true;
    RemoteClient client(config);
    client.setIdentity("agent-ch", "host-ch");
    client.setAuthToken("tok-1");
    ASSERT_TRUE(client.start());
    ASSERT_TRUE(waitServerMessages(1));

    auto registerMsg = nlohmann::json::parse(serverMessages_[0]->body);
    EXPECT_EQ(registerMsg["challenge"], true);
    EXPECT_FALSE(registerMsg.contains("token"));

    client.stop();
}

TEST_F(RemoteClientTest, RegisterDefaultModeNeverCarriesChallengeFlag) {
    ASSERT_TRUE(startServer());
    RemoteClient client(makeConfig());
    client.setAuthToken("tok-1");
    ASSERT_TRUE(client.start());
    ASSERT_TRUE(waitServerMessages(1));

    auto registerMsg = nlohmann::json::parse(serverMessages_[0]->body);
    EXPECT_EQ(registerMsg["token"], "tok-1");
    EXPECT_FALSE(registerMsg.contains("challenge"));

    client.stop();
}

TEST_F(RemoteClientTest, AuthChallengeRequestAnsweredWithHmac) {
    ASSERT_TRUE(startServer());
    auto config = makeConfig();
    config.useChallengeAuth = true;
    client_ = std::make_unique<RemoteClient>(config);
    client_->setAuthToken("tok-1");
    ASSERT_TRUE(client_->start());
    ASSERT_TRUE(waitServerMessages(1)); // agent.register

    // server 下发 auth.challenge Request；client 应回 Response（同 seq）携 hmac
    const std::string nonce = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
    auto req = Message::create(MessageType::Request,
        nlohmann::json{{"type", "auth.challenge"}, {"nonce", nonce}}.dump());
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], req));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr& resp = serverMessages_[1];
    EXPECT_EQ(resp->header.type, MessageType::Response);
    auto body = nlohmann::json::parse(resp->body);
    EXPECT_EQ(body["hmac"],
        "aa1b9280bd896237986b9cc680446c0833f6cedeaffe1b2e47b0bcd337f1f4d6");

    client_->stop();
}

TEST_F(RemoteClientTest, AuthChallengeWithoutTokenAnswersEmptyHmac) {
    ASSERT_TRUE(startServer());
    auto config = makeConfig();
    config.useChallengeAuth = true;
    client_ = std::make_unique<RemoteClient>(config);
    // 不设 token
    ASSERT_TRUE(client_->start());
    ASSERT_TRUE(waitServerMessages(1));

    const std::string nonce = "aabbccdd";
    auto req = Message::create(MessageType::Request,
        nlohmann::json{{"type", "auth.challenge"}, {"nonce", nonce}}.dump());
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], req));
    ASSERT_TRUE(waitServerMessages(2));

    auto body = nlohmann::json::parse(serverMessages_[1]->body);
    EXPECT_EQ(body["hmac"], "");

    client_->stop();
}

