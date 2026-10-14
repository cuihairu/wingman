// asset.sync 端侧落盘（A3 后续增量）：lib/wingman 的 RpcDispatcher +
// registerAssetHandlers 经 wingman::androidagent cherry-pick 目标与 NDK
// 同源编译（libs/androidagent/CMakeLists.txt 清单）。本文件在桌面侧
// 引用这两个符号并驱动一次完整落盘，防止 cherry-pick 清单回退——
// 清单被裁时此处直接编译/链接失败，而非等 nightly NDK 构建才暴露。
// 行为矩阵（名称白名单/上限/原子写）的权威用例在
// lib/wingman/tests/rpc_test.cpp 的 AssetRpcTest，此处不重复。
#include <gtest/gtest.h>

#include "wingman/crypt.hpp"
#include "wingman/rpc/asset_handler.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;
using wingman::crypt::base64Encode;
using wingman::crypt::sha256;
using wingman::rpc::registerAssetHandlers;
using wingman::rpc::RpcDispatcher;

class AndroidAssetChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        assetsDir =
            (std::filesystem::path(::testing::TempDir()) / "androidagent-assets").string();
        std::filesystem::remove_all(assetsDir);
        registerAssetHandlers(dispatcher, assetsDir);
    }

    // 与 AndroidAgent 通道同形的请求构造：method=asset.sync + params
    //（apps/android/cpp/agent 经 dispatchViaRpcDispatcher 转发到这里）
    json sync(const json& params) {
        json req = {{"method", "asset.sync"}, {"id", "1"}, {"params", params}};
        return json::parse(dispatcher.dispatch(req.dump()));
    }

    static std::string b64(const std::string& raw) {
        return base64Encode({raw.begin(), raw.end()});
    }

    RpcDispatcher dispatcher;
    std::string assetsDir;
};

TEST_F(AndroidAssetChannelTest, SyncWritesFileAndReturnsPath) {
    const std::string content = "fake-template-bytes";
    json response = sync({
        {"name", "btn_ok.png"},
        {"contentBase64", b64(content)},
        {"sha256", sha256(content)},
    });
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const std::string path = response["data"]["result"]["path"].get<std::string>();
    EXPECT_EQ(std::filesystem::path(path).filename(), "btn_ok.png");
    std::ifstream in(path, std::ios::binary);
    EXPECT_EQ((std::string{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()}),
              content);
}

TEST_F(AndroidAssetChannelTest, SyncRejectsInvalidName) {
    json response = sync({
        {"name", "../escape.png"},
        {"contentBase64", b64("x")},
    });
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "invalid asset name");
    // 拒绝路径不产生任何落盘
    EXPECT_FALSE(std::filesystem::exists(assetsDir));
}

TEST_F(AndroidAssetChannelTest, SyncShaMismatchRejects) {
    json response = sync({
        {"name", "btn.png"},
        {"contentBase64", b64("content")},
        {"sha256", std::string(64, '0')},
    });
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "sha256 mismatch");
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(assetsDir) / "btn.png"));
}

} // namespace
