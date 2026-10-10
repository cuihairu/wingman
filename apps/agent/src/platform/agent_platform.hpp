// Agent 平台身份薄层：可执行名/进程扫描名/stub 候选路径随平台不同
// （Windows 带 .exe 后缀，MSVC 构建树 stub 多 Release/Debug 配置目录）。
// 平台分支由 CMake 按目录选源收敛（platform/win/ vs platform/posix/），
// 本头文件与调用点保持零平台宏（薄层纪律，见
// docs/platform-abstraction-design.md §8）。
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace wingman::runtime::platform {

// 进程扫描名列表（stop/status 命令的查找口径）
std::vector<std::string> agentProcessNames();

// build 命令的 stub 可执行文件候选路径（按优先级排列）
std::vector<std::filesystem::path> stubCandidatePaths();

// 嵌入资源（PE RCDATA）探测：Windows FindResource/SizeofResource；非 Windows
// 无嵌入资源，恒 exists=false
struct EmbeddedResourceProbe {
    bool exists = false;
    uint32_t size = 0;
};
EmbeddedResourceProbe probeEmbeddedResource();

// 嵌入资源字节读取；无资源/读取失败返回空
std::vector<uint8_t> readEmbeddedResource();

// 可执行文件绝对路径（含文件名）；解析失败返回空串
std::string executablePath();

// PE 资源写入（BeginUpdateResource/UpdateResource）：Windows 专有；
// 非 Windows 无对应物，明确失败返回 false（不静默产出未嵌脚本的产物）
bool updatePeResource(const std::string& outputPath, const std::vector<uint8_t>& resourceData);

// PE 图标替换：Windows 专有；非 Windows 跳过（返回 true）
bool replacePeIcon(const std::string& outputPath, const std::string& iconPath);

// PE 版本信息写入（VS_VERSIONINFO）：Windows 专有；非 Windows 跳过（返回 true）
bool setPeVersionInfo(const std::string& outputPath,
                      const std::string& appName,
                      const std::string& appVersion);

} // namespace wingman::runtime::platform
