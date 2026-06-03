#include <gtest/gtest.h>

#include "wingman/runtime/cli.hpp"
#include "wingman/runtime/commands/build_command.hpp"
#include "wingman/runtime/commands/script_command.hpp"
#include "wingman/runtime/commands/stop_command.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>

namespace wingman::runtime::commands {
extern std::string g_lastStartConfigPath;
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

TEST(RuntimeCommandTest, StopCommandReturnsSuccessEvenWhenNoProcessFound) {
    EXPECT_EQ(wingman::runtime::commands::stopCommand(), 0);
}

TEST(RuntimeCommandTest, StatusCommandReturnsKnownExitCode) {
    const int code = wingman::runtime::commands::statusCommand();
    EXPECT_TRUE(code == 0 || code == 1);
}
