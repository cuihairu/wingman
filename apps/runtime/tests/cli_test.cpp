#include <gtest/gtest.h>

#include "wingman/runtime/cli.hpp"
#include "wingman/runtime/commands/build_command.hpp"
#include "wingman/runtime/commands/script_command.hpp"
#include "wingman/runtime/commands/stop_command.hpp"
#include "wingman/runtime/config.hpp"
#include "wingman/runtime/packer.hpp"
#include "wingman/runtime/resource_loader.hpp"
#include "wingman/runtime/runtime_context.hpp"
#include "wingman/lua/lua_script_engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
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

// ========== script 命令执行面（成功/运行时错误/不可编译） ==========
// 既有用例只测过「文件不存在」一条腿；命令主体（引擎装配、loadScript、
// runScript、卸载收尾）零驱动。Lua 引擎经 registerLuaEngine() 惰性注册，
// 与 rpc_ipc_test 的接线方式一致。

namespace {

std::filesystem::path writeTempScript(const std::string& name, const std::string& content) {
    const auto dir = makeTempDir();
    const auto path = dir / name;
    std::ofstream file(path);
    file << content;
    return path;
}

} // namespace

TEST(RuntimeCommandTest, ScriptCommandRunsLuaScriptToCompletion) {
    wingman::lua::registerLuaEngine();
    const auto script = writeTempScript("cli_ok.lua", "print('cli-ok')\n");
    EXPECT_EQ(wingman::runtime::commands::scriptCommand(script.string(), {}), 0);
}

TEST(RuntimeCommandTest, ScriptCommandPassesArgumentsAsScriptEnv) {
    // 位置参数进 config.env["arg1"..]（脚本经 env 读取；这里只锁「带参执行
    // 不破坏成功路径」——env 的读取面属脚本引擎语义，不在本命令职责内）
    wingman::lua::registerLuaEngine();
    const auto script = writeTempScript("cli_args.lua", "print('cli-args')\n");
    EXPECT_EQ(wingman::runtime::commands::scriptCommand(script.string(), {"alpha", "beta"}), 0);
}

TEST(RuntimeCommandTest, ScriptCommandRejectsUnreadableScriptSource) {
    // 存在但不可作为脚本加载的路径（目录）：exists 通过、loadScript 失败——
    // 与语法错误不同（引擎把编译推迟到运行，语法错走 runScript 失败腿）
    wingman::lua::registerLuaEngine();
    const auto dir = makeTempDir();
    EXPECT_EQ(wingman::runtime::commands::scriptCommand(dir.string(), {}), 1);
}

TEST(RuntimeCommandTest, ScriptCommandReportsRuntimeError) {
    wingman::lua::registerLuaEngine();
    const auto script = writeTempScript("cli_error.lua", "error('boom-from-cli')\n");
    EXPECT_EQ(wingman::runtime::commands::scriptCommand(script.string(), {}), 1);
}

TEST(RuntimeCommandTest, ScriptCommandRejectsUncompilableScript) {
    wingman::lua::registerLuaEngine();
    // 语法错误在 loadScript 阶段即失败（runScript 不应被触达）
    const auto script = writeTempScript("cli_syntax.lua", "local local\n");
    EXPECT_EQ(wingman::runtime::commands::scriptCommand(script.string(), {}), 1);
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

TEST(RuntimeCommandTest, BuildCommandWithStubFailsGracefullyOnNonPEHost) {
    // resolveStubPath 只要求候选路径存在（CWD 首选 "wingman-runtime"）；
    // 找到 stub 后走完整打包管线：Linux 上资源嵌入明确不支持（ELF 容器
    // 未实现），build() 优雅失败 → "Build failed" 退出码 1（不抛异常）。
    // 既有用例只测过「脚本缺失」「stub 缺失」两条前置拒绝腿。
    const auto tempDir = makeTempDir();
    WorkingDirectoryGuard guard(tempDir);

    const auto scriptPath = tempDir / "test.lua";
    std::ofstream script(scriptPath);
    script << "print('ok')";
    script.close();

    std::ofstream stub(tempDir / "wingman-runtime", std::ios::binary);
    stub << "ELF-placeholder";
    stub.close();

    wingman::runtime::commands::BuildOptions options;
    options.scriptPath = scriptPath.string();
    options.outputPath = (tempDir / "out" / "app.bin").string();  // 带父目录 → create_directories
    options.iconPath = (tempDir / "icon.ico").string();          // 非空 → 图标日志行

    EXPECT_EQ(wingman::runtime::commands::buildCommand(options), 1);
}

TEST(RuntimeCommandTest, BuildOptionsDefaultToUnencryptedResources) {
    wingman::runtime::commands::BuildOptions commandOptions;
    wingman::runtime::PackerOptions packerOptions;

    EXPECT_FALSE(commandOptions.encrypt);
    EXPECT_FALSE(packerOptions.encrypt);
    // 默认无口令 —— 加密是显式选择，且没有口令时打包会被拒绝
    EXPECT_TRUE(commandOptions.password.empty());
    EXPECT_TRUE(packerOptions.password.empty());
}

// 加密资源一度在 Packer::build() 里被整体拒绝（「loader 还不支持」）。
// loader 现已支持口令派生密钥，那组语义随之作废：下面两条改写为
// 「有口令放行、无口令硬拒」，字节级往返与错口令见 resource_pack_test.cpp。

TEST(RuntimeCommandTest, PackerRejectsEncryptionWithoutPassword) {
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
    options.password.clear();

    wingman::runtime::Packer packer(options);
    const auto result = packer.build();

    // 没有口令的加密产物永久打不开，故拒绝要发生在复制 stub 之前
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("requires a password"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(outputPath));
}

TEST(RuntimeCommandTest, PackerAcceptsEncryptedResourcesWhenGivenAPassword) {
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
    options.password = "hunter2";

    wingman::runtime::Packer packer(options);
    const auto result = packer.build();

    // 加密不再构成拒绝理由：剩下的唯一平台边界是 PE 资源写入本身
    // （BeginUpdateResource/UpdateResource 无对应物），与非加密用例的边界一致。
#ifdef _WIN32
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(outputPath));
#else
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "Failed to embed script resource");
    EXPECT_FALSE(std::filesystem::exists(outputPath));
#endif
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

// ========== ResourceLoader: Lua 字节码检测 ==========

TEST(ResourceLoaderTest, LooksLikeLuaBytecodeDetectsLua54Chunk) {
    // Lua 5.4 undump 签名：ESC 'L' 'u' 'a' + 版本字节 0x54 + 格式版本 0x00
    const std::vector<uint8_t> data = {0x1B, 'L', 'u', 'a', 0x54, 0x00, 0x19, 0x93};
    EXPECT_TRUE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode(data));
}

TEST(ResourceLoaderTest, LooksLikeLuaBytecodeDetectsLuaJitChunk) {
    const std::vector<uint8_t> data = {0x1B, 'L', 'J', 0x01, 0x04, 0x08};
    EXPECT_TRUE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode(data));
}

TEST(ResourceLoaderTest, LooksLikeLuaBytecodeDetectsOlderLuaVersions) {
    // Lua 5.1/5.2/5.3 版本字节不同，但前缀签名一致
    for (const uint8_t version : {0x51, 0x52, 0x53}) {
        const std::vector<uint8_t> data = {0x1B, 'L', 'u', 'a', version};
        EXPECT_TRUE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode(data));
    }
}

TEST(ResourceLoaderTest, LooksLikeLuaBytecodeRejectsPlainTextSource) {
    const std::string source = "print('ok')";
    const std::vector<uint8_t> data(source.begin(), source.end());
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode(data));
}

TEST(ResourceLoaderTest, LooksLikeLuaBytecodeRejectsShortAndEmptyPayloads) {
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode({}));
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode({0x1B}));
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode({0x1B, 'L'}));
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode({0x1B, 'L', 'u'}));
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode({0x1B, 'L', 'J'}));
}

TEST(ResourceLoaderTest, LooksLikeLuaBytecodeRejectsLuaWithoutEscape) {
    // 明文恰好包含 "Lua" 但缺 ESC 前缀
    const std::vector<uint8_t> data = {'L', 'u', 'a', 0x54};
    EXPECT_FALSE(wingman::runtime::ResourceLoader::looksLikeLuaBytecode(data));
}
