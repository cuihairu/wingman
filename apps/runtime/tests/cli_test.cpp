#include <gtest/gtest.h>

#include "wingman/runtime/cli.hpp"
#include "wingman/runtime/commands/build_command.hpp"
#include "wingman/runtime/commands/script_command.hpp"
#include "wingman/runtime/commands/stop_command.hpp"
#include "wingman/runtime/config.hpp"
#include "wingman/runtime/packer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>

namespace wingman::runtime::commands {
extern std::string g_lastStartConfigPath;
extern bool g_lastStartForceStandalone;
}

namespace {

class WorkingDirectoryGuard {
public:
    explicit WorkingDirectoryGuard(const std::filesystem::path& path)
        : original_(std::filesystem::current_path()) {
        std::filesystem::current_path(path);
    }

    ~WorkingDirectoryGuard() {
        std::filesystem::current_path(original_);
    }

private:
    std::filesystem::path original_;
};

std::filesystem::path makeTempDir() {
    const auto path = std::filesystem::temp_directory_path() /
        ("wingman-runtime-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
    return path;
}

} // namespace

TEST(RuntimeCliTest, HelpCommandReturnsSuccess) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"help"}), 0);
}

TEST(RuntimeCliTest, EmptyArgsReturnFailure) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({}), 1);
}

TEST(RuntimeCliTest, UnknownCommandReturnsFailure) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"unknown"}), 1);
}

TEST(RuntimeCliTest, StartDispatchUsesDefaultConfig) {
    wingman::runtime::commands::g_lastStartConfigPath.clear();
    EXPECT_EQ(wingman::runtime::dispatchCommand({"start"}), 42);
    EXPECT_EQ(wingman::runtime::commands::g_lastStartConfigPath, "agent.toml");
}

TEST(RuntimeCliTest, StartDispatchUsesExplicitConfig) {
    wingman::runtime::commands::g_lastStartConfigPath.clear();
    EXPECT_EQ(wingman::runtime::dispatchCommand({"start", "--config", "custom.toml"}), 42);
    EXPECT_EQ(wingman::runtime::commands::g_lastStartConfigPath, "custom.toml");
}

TEST(RuntimeCliTest, StartDispatchUsesStandaloneFlag) {
    wingman::runtime::commands::g_lastStartForceStandalone = false;
    EXPECT_EQ(wingman::runtime::dispatchCommand({"start", "--standalone"}), 42);
    EXPECT_TRUE(wingman::runtime::commands::g_lastStartForceStandalone);
}

TEST(RuntimeCliTest, StartDispatchRejectsUnknownOption) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"start", "--bad"}), 1);
}

TEST(RuntimeCliTest, StopDispatchRejectsArguments) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"stop", "extra"}), 1);
}

TEST(RuntimeCliTest, StatusDispatchRejectsArguments) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"status", "extra"}), 1);
}

TEST(RuntimeCliTest, ScriptDispatchRequiresPath) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"script"}), 1);
}

TEST(RuntimeCliTest, BuildDispatchRequiresMandatoryFlags) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"build"}), 1);
}

TEST(RuntimeCliTest, BuildDispatchRejectsUnknownOption) {
    EXPECT_EQ(wingman::runtime::dispatchCommand({"build", "--bad"}), 1);
}

TEST(RuntimeCommandTest, ScriptCommandFailsWhenFileIsMissing) {
    EXPECT_EQ(wingman::runtime::commands::scriptCommand("/definitely/missing.lua", {}), 1);
}

TEST(RuntimeCommandTest, BuildCommandFailsWhenScriptIsMissing) {
    wingman::runtime::commands::BuildOptions options;
    options.scriptPath = "/definitely/missing.lua";
    options.outputPath = "out.bin";
    EXPECT_EQ(wingman::runtime::commands::buildCommand(options), 1);
}

TEST(RuntimeCommandTest, BuildCommandFailsWhenRuntimeStubIsMissing) {
    const auto tempDir = makeTempDir();
    WorkingDirectoryGuard guard(tempDir);

    const auto scriptPath = tempDir / "test.lua";
    std::ofstream script(scriptPath);
    script << "print('ok')";
    script.close();

    wingman::runtime::commands::BuildOptions options;
    options.scriptPath = scriptPath.string();
    options.outputPath = (tempDir / "out.bin").string();

    EXPECT_EQ(wingman::runtime::commands::buildCommand(options), 1);
}

TEST(RuntimeCommandTest, BuildOptionsDefaultToUnencryptedResources) {
    wingman::runtime::commands::BuildOptions commandOptions;
    wingman::runtime::PackerOptions packerOptions;

    EXPECT_FALSE(commandOptions.encrypt);
    EXPECT_FALSE(packerOptions.encrypt);
}

TEST(RuntimeCommandTest, PackerRejectsEncryptedResourcesUntilLoaderSupportsThem) {
    const auto tempDir = makeTempDir();
    const auto scriptPath = tempDir / "test.lua";
    const auto stubPath = tempDir / "stub.bin";
    const auto outputPath = tempDir / "out.bin";

    {
        std::ofstream script(scriptPath);
        script << "print('ok')";
    }
    {
        std::ofstream stub(stubPath, std::ios::binary);
        stub << "stub";
    }

    wingman::runtime::PackerOptions options;
    options.scriptPath = scriptPath.string();
    options.stubPath = stubPath.string();
    options.outputPath = outputPath.string();
    options.encrypt = true;

    wingman::runtime::Packer packer(options);
    const auto result = packer.build();

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("not supported"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(outputPath));
}

TEST(RuntimeCommandTest, PackerRemovesPartialOutputWhenResourceEmbeddingFails) {
    const auto tempDir = makeTempDir();
    const auto scriptPath = tempDir / "test.lua";
    const auto stubPath = tempDir / "stub.bin";
    const auto outputPath = tempDir / "out.bin";

    {
        std::ofstream script(scriptPath);
        script << "print('ok')";
    }
    {
        std::ofstream stub(stubPath, std::ios::binary);
        stub << "stub";
    }

    wingman::runtime::PackerOptions options;
    options.scriptPath = scriptPath.string();
    options.stubPath = stubPath.string();
    options.outputPath = outputPath.string();
    options.encrypt = false;

    wingman::runtime::Packer packer(options);
    const auto result = packer.build();

#ifdef _WIN32
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(outputPath));
#else
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "Failed to embed script resource");
    EXPECT_FALSE(std::filesystem::exists(outputPath));
#endif
}

TEST(RuntimeCommandTest, StopCommandReturnsSuccessEvenWhenNoProcessFound) {
    EXPECT_EQ(wingman::runtime::commands::stopCommand(), 0);
}

TEST(RuntimeCommandTest, StatusCommandReturnsKnownExitCode) {
    const int code = wingman::runtime::commands::statusCommand();
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST(RuntimeConfigTest, ParsesQuotedStringsAndInlineComments) {
    const auto config = wingman::runtime::AgentConfig::loadFromString(R"(
        enable_remote = true # connect to orchestrator
        [remote]
        server_ip = "10.0.0.5"
        server_port = 9527
        [standalone]
        script_dir = "scripts/local # not a comment"
    )");

    EXPECT_TRUE(config.enableRemote);
    EXPECT_EQ(config.remoteClient.serverIp, "10.0.0.5");
    EXPECT_EQ(config.remoteClient.serverPort, 9527);
    EXPECT_EQ(config.standalone.scriptDir, "scripts/local # not a comment");
}
