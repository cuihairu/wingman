// POSIX 实现（Linux/macOS）：无 .exe 后缀，CMake 单配置构建树布局
#include "platform/agent_platform.hpp"

#include <spdlog/spdlog.h>
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

bool updatePeResource(const std::string& /*outputPath*/,
                      const std::vector<uint8_t>& /*resourceData*/) {
    // PE 资源写入（BeginUpdateResource/UpdateResource）不存在非 Windows 对应物：
    // ELF 侧要产出可分发的自包含可执行文件得另设容器方案，未实现即明确失败，
    // 不静默写出一份「看起来成功、实际没嵌脚本」的产物。
    spdlog::error("Resource update not supported on this platform");
    return false;
}

bool replacePeIcon(const std::string& /*outputPath*/, const std::string& /*iconPath*/) {
    spdlog::warn("Icon replacement not supported on this platform");
    return true;
}

bool setPeVersionInfo(const std::string& /*outputPath*/,
                      const std::string& /*appName*/,
                      const std::string& /*appVersion*/) {
    spdlog::warn("Version info not supported on this platform");
    return true;
}

} // namespace wingman::runtime::platform
