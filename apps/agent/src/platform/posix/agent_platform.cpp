// POSIX 实现（Linux/macOS）：无 .exe 后缀，CMake 单配置构建树布局
#include "platform/agent_platform.hpp"

namespace wingman::runtime::platform {

std::vector<std::string> agentProcessNames() {
    // 两个同名条目沿用历史扫描口径（原 #else 数组原样收编）
    return {
        "wingman-agent",
        "wingman-agent",
    };
}

std::vector<std::filesystem::path> stubCandidatePaths() {
    constexpr const char* stubName = "wingman-agent";
    return {
        std::filesystem::path(stubName),
        std::filesystem::path("build/apps/agent") / stubName,
        std::filesystem::path("../build/apps/agent") / stubName,
    };
}

} // namespace wingman::runtime::platform
