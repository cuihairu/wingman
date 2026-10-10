// POSIX 实现（Linux/macOS）：无 .exe 后缀，CMake 单配置构建树布局
#include "platform/agent_platform.hpp"

#include <limits.h>
#include <unistd.h>

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

EmbeddedResourceProbe probeEmbeddedResource() {
    // POSIX 无 PE 嵌入资源容器，恒无资源
    return {};
}

std::vector<uint8_t> readEmbeddedResource() {
    return {};
}

std::string executablePath() {
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        return std::string(path, count);
    }
    return {};
}

} // namespace wingman::runtime::platform
