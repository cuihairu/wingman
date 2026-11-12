// mcp 子命令端到端：in-process 腿无条件（直驱 McpServer::run，Windows CI
// codecov 口径）；POSIX 真子进程腿（fork/execvp + 双 pipe——wingman::Process
// 无 stdio 管道能力，不可用）验证真进程 stdout 纯净：任何 spdlog 日志漏进
// stdout 都会让「每行都是合法 JSON」断言炸掉（日志走 stderr 是 mcp 模式
// main.cpp 特意配置的）。二进制定位：env WINGMAN_AGENT_BIN → argv[0] 同
// 目录兄弟候选（build-tests 布局 tests/ 平级上层），找不到 GTEST_SKIP。

#include <gtest/gtest.h>

#include "wingman/mcp/mcp_server.hpp"
#include "wingman/runtime/resource_loader.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

using nlohmann::json;

namespace {

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

std::string locateAgentBinary() {
    if (const char* env = std::getenv("WINGMAN_AGENT_BIN")) {
        if (std::filesystem::exists(env)) return env;
    }
    std::error_code ec;
    const std::string exe = wingman::runtime::ResourceLoader::getExecutablePath();
    if (!exe.empty()) {
        const std::filesystem::path dir = std::filesystem::path(exe).parent_path();
        for (const char* candidate : {"../wingman-agent", "wingman-agent"}) {
            const std::filesystem::path p = dir / candidate;
            if (std::filesystem::exists(p, ec)) return p.string();
        }
    }
    return {};
}

} // namespace

// ========== in-process 腿（跨平台，无条件） ==========

TEST(McpE2eTest, InProcessStdioRoundTrip) {
    wingman::mcp::McpServer server("wingman", "0.0.0-e2e");

    std::ostringstream frames;
    frames << request(1, "initialize").dump() << '\n';
    frames << "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n";
    frames << request(2, "tools/list").dump() << '\n';
    frames << request(3, "ping").dump() << '\n';

    std::istringstream in(frames.str());
    std::ostringstream out;
    server.run(in, out);

    const auto lines = splitLines(out.str());
    ASSERT_EQ(lines.size(), 3u); // notification 零响应

    const json init = json::parse(lines[0]);
    EXPECT_EQ(init.at("id").get<int>(), 1);
    EXPECT_EQ(init.at("result").at("protocolVersion").get<std::string>(), "2025-11-25");
    EXPECT_TRUE(init.at("result").at("capabilities").contains("tools"));
    EXPECT_EQ(init.at("result").at("serverInfo").at("name").get<std::string>(), "wingman");

    const json tools = json::parse(lines[1]).at("result").at("tools");
    EXPECT_EQ(tools.size(), 7u);

    const json ping = json::parse(lines[2]);
    EXPECT_TRUE(ping.at("result").empty());
}

// ========== 真子进程腿（POSIX）：stdio 全链 + stdout 纯净 ==========

#ifndef _WIN32
TEST(McpE2eTest, StdioRoundTripRealProcess) {
    const std::string binary = locateAgentBinary();
    if (binary.empty()) {
        GTEST_SKIP() << "wingman-agent binary not found (set WINGMAN_AGENT_BIN)";
    }

    int toChild[2];
    int fromChild[2];
    ASSERT_EQ(::pipe(toChild), 0);
    ASSERT_EQ(::pipe(fromChild), 0);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        // 子进程：stdin/stdout 接管管道，stderr 留给测试进程的日志面
        ::dup2(toChild[0], STDIN_FILENO);
        ::dup2(fromChild[1], STDOUT_FILENO);
        ::close(toChild[0]);
        ::close(toChild[1]);
        ::close(fromChild[0]);
        ::close(fromChild[1]);
        ::execl(binary.c_str(), binary.c_str(), "mcp", static_cast<char*>(nullptr));
        ::_exit(127); // exec 失败
    }

    ::close(toChild[0]);
    ::close(fromChild[1]);

    const std::string payload =
        request(1, "initialize").dump() + "\n" +
        request(2, "tools/list").dump() + "\n" +
        request(3, "tools/call",
                {{"name", "wingman_uia_find"},
                 {"arguments", {{"name", "anything"}}}})
            .dump() +
        "\n" +
        request(4, "ping").dump() + "\n";
    ASSERT_EQ(::write(toChild[1], payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    ::close(toChild[1]); // EOF → mcp run 循环退出

    std::string stdoutData;
    char buffer[4096];
    ssize_t n = 0;
    while ((n = ::read(fromChild[0], buffer, sizeof(buffer))) > 0) {
        stdoutData.append(buffer, static_cast<size_t>(n));
    }
    ::close(fromChild[0]);

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status)) << "child crashed";
    EXPECT_EQ(WEXITSTATUS(status), 0);

    // 核心断言：子进程 stdout 每一行都是合法 JSON-RPC（日志漏进 stdout 即炸）
    const auto lines = splitLines(stdoutData);
    ASSERT_EQ(lines.size(), 4u);
    std::vector<json> frames;
    for (const auto& line : lines) {
        ASSERT_NO_THROW(frames.push_back(json::parse(line))) << "non-JSON stdout line: " << line;
    }

    EXPECT_EQ(frames[0].at("id").get<int>(), 1);
    EXPECT_EQ(frames[0].at("result").at("serverInfo").at("name").get<std::string>(), "wingman");
    EXPECT_EQ(frames[1].at("id").get<int>(), 2);
    EXPECT_EQ(frames[1].at("result").at("tools").size(), 7u);
    // Linux 无 UIA 后端：明确 isError，不假装成功
    EXPECT_EQ(frames[2].at("id").get<int>(), 3);
    EXPECT_TRUE(frames[2].at("result").at("isError").get<bool>());
    EXPECT_NE(frames[2].at("result")
                  .at("content")[0]
                  .at("text")
                  .get<std::string>()
                  .find("not supported"),
              std::string::npos);
    EXPECT_EQ(frames[3].at("id").get<int>(), 4);
    EXPECT_TRUE(frames[3].at("result").empty());
}

TEST(McpE2eTest, StdioRejectsArguments) {
    // mcp 拒收参数：exit 1、stdout 零输出（协议面纯净）、报错走 stderr
    const std::string binary = locateAgentBinary();
    if (binary.empty()) {
        GTEST_SKIP() << "wingman-agent binary not found (set WINGMAN_AGENT_BIN)";
    }

    int fromChild[2];
    int errPipe[2];
    ASSERT_EQ(::pipe(fromChild), 0);
    ASSERT_EQ(::pipe(errPipe), 0);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        ::dup2(fromChild[1], STDOUT_FILENO);
        ::dup2(errPipe[1], STDERR_FILENO);
        ::close(fromChild[0]);
        ::close(fromChild[1]);
        ::close(errPipe[0]);
        ::close(errPipe[1]);
        ::execl(binary.c_str(), binary.c_str(), "mcp", "extra", static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(fromChild[1]);
    ::close(errPipe[1]);

    std::string stdoutData;
    char buffer[1024];
    ssize_t n = 0;
    while ((n = ::read(fromChild[0], buffer, sizeof(buffer))) > 0) {
        stdoutData.append(buffer, static_cast<size_t>(n));
    }
    ::close(fromChild[0]);

    std::string stderrData;
    while ((n = ::read(errPipe[0], buffer, sizeof(buffer))) > 0) {
        stderrData.append(buffer, static_cast<size_t>(n));
    }
    ::close(errPipe[0]);

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 1);
    EXPECT_TRUE(stdoutData.empty());
    EXPECT_NE(stderrData.find("mcp does not accept arguments"), std::string::npos);
}
#endif
