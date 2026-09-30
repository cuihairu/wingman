// ResourceLoader 实例接口补测（覆盖率轮 2026-09-30）。
//
// 既有 resource_pack_test.cpp 只直测静态字节级入口（loadScriptFromBytes /
// Packer 往返），实例面（构造探测 / 错误回调 / 资源信息 / loadScript）零驱动。
// 测试二进制不是 Packer 产物，不含 PACK_PE_RESOURCE_ID 资源，Linux 与
// Windows CI 上探测结果一致为「无嵌入脚本」，可全平台断言同一路径。
//
// Linux 上的既定口径（resource_loader.cpp 的 #ifdef 分派）：
// - detectResource()/readResourceData() 非 Windows 分支恒 false/{}
//   → loadScript 必走「No embedded script found」早退，PE 读取腿不可达；
// - getExecutablePath() 非 Windows 走 /proc/self/exe。
#include <gtest/gtest.h>

#include "wingman/runtime/resource_loader.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace wingman::runtime {
namespace {

TEST(ResourceLoaderInterfaceTest, ConstructionDetectsNoEmbeddedScriptInTestBinary) {
    ResourceLoader loader;
    // 测试二进制未嵌入脚本资源（Packer 产物才有），探测必须落「不存在」
    EXPECT_FALSE(loader.hasEmbeddedScript());
}

TEST(ResourceLoaderInterfaceTest, ResourceInfoDefaultsWhenNothingEmbedded) {
    ResourceLoader loader;
    const ResourceInfo info = loader.getResourceInfo();
    EXPECT_FALSE(info.exists);
    EXPECT_EQ(info.version, 0u);
    EXPECT_EQ(info.originalSize, 0u);
    EXPECT_EQ(info.compressedSize, 0u);
    EXPECT_FALSE(info.encrypted);
    EXPECT_FALSE(info.compressed);
}

TEST(ResourceLoaderInterfaceTest, GetExecutablePathResolvesToExistingFile) {
    const std::string path = ResourceLoader::getExecutablePath();
    ASSERT_FALSE(path.empty());
    // 构造期缓存的是同一条解析（/proc/self/exe / GetModuleFileNameW）
    EXPECT_TRUE(std::filesystem::exists(path)) << "path: " << path;
}

TEST(ResourceLoaderInterfaceTest, LoadScriptWithoutEmbeddedScriptReportsViaCallback) {
    ResourceLoader loader;
    std::vector<std::string> errors;
    loader.setErrorCallback([&errors](const std::string& message) {
        errors.push_back(message);
    });

    const auto script = loader.loadScript("any-password");
    EXPECT_FALSE(script.has_value());
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors.front(), "No embedded script found");
}

TEST(ResourceLoaderInterfaceTest, LoadScriptWithoutCallbackFailsSilently) {
    ResourceLoader loader;
    // 未装回调的失败路径不得崩溃，仍返回 nullopt
    const auto script = loader.loadScript();
    EXPECT_FALSE(script.has_value());
}

TEST(ResourceLoaderInterfaceTest, ErrorCallbackIsReplaceable) {
    ResourceLoader loader;
    std::string first;
    std::string second;
    loader.setErrorCallback([&first](const std::string& message) { first = message; });
    loader.setErrorCallback([&second](const std::string& message) { second = message; });

    EXPECT_FALSE(loader.loadScript("").has_value());
    EXPECT_TRUE(first.empty()) << "旧回调应被替换，不再接收";
    EXPECT_EQ(second, "No embedded script found");
}

TEST(ResourceLoaderInterfaceTest, RepeatedLoadScriptKeepsFailingConsistently) {
    ResourceLoader loader;
    int calls = 0;
    loader.setErrorCallback([&calls](const std::string&) { ++calls; });

    EXPECT_FALSE(loader.loadScript("a").has_value());
    EXPECT_FALSE(loader.loadScript("b").has_value());
    EXPECT_EQ(calls, 2);
    // 探测结果不随加载尝试漂移
    EXPECT_FALSE(loader.hasEmbeddedScript());
}

} // namespace
} // namespace wingman::runtime
