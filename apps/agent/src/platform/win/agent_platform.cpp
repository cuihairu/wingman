// Windows 实现：可执行名带 .exe，MSVC 构建树 stub 在 Release/Debug 配置目录
#include "platform/agent_platform.hpp"

#include "wingman/runtime/resource_pack.hpp"
#include <windows.h>
#include <winuser.h>

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

EmbeddedResourceProbe probeEmbeddedResource() {
    EmbeddedResourceProbe probe;
    HMODULE hModule = GetModuleHandleW(NULL);
    if (!hModule) {
        return probe;
    }
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(PACK_PE_RESOURCE_ID), RT_RCDATA);
    if (!hRes) {
        return probe;
    }
    probe.size = SizeofResource(hModule, hRes);
    probe.exists = true;
    return probe;
}

std::vector<uint8_t> readEmbeddedResource() {
    HMODULE hModule = GetModuleHandleW(NULL);
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(PACK_PE_RESOURCE_ID), RT_RCDATA);
    if (!hRes) {
        return {};
    }

    DWORD size = SizeofResource(hModule, hRes);
    HGLOBAL hLoaded = LoadResource(hModule, hRes);
    if (!hLoaded) {
        return {};
    }

    void* pData = LockResource(hLoaded);
    if (!pData) {
        return {};
    }

    return std::vector<uint8_t>(static_cast<uint8_t*>(pData),
                                static_cast<uint8_t*>(pData) + size);
}

std::string executablePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    // 转换为窄字符
    int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);
        return result;
    }
    return {};
}

} // namespace wingman::runtime::platform
