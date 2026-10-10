// Windows 实现：可执行名带 .exe，MSVC 构建树 stub 在 Release/Debug 配置目录
#include "platform/agent_platform.hpp"

namespace wingman::runtime::platform {

std::vector<std::string> agentProcessNames() {
    // 两个同名条目沿用历史扫描口径（原 #ifdef 数组原样收编）
    return {
        "wingman-agent.exe",
        "wingman-agent.exe",
    };
}

std::vector<std::filesystem::path> stubCandidatePaths() {
    constexpr const char* stubName = "wingman-agent.exe";
    return {
        std::filesystem::path(stubName),
        std::filesystem::path("build/apps/agent/Release") / stubName,
        std::filesystem::path("../build/apps/agent/Release") / stubName,
        std::filesystem::path("build/apps/agent/Debug") / stubName,
        std::filesystem::path("../build/apps/agent/Debug") / stubName,
    };
}

} // namespace wingman::runtime::platform
