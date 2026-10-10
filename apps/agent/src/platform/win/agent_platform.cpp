// Windows 实现：可执行名带 .exe，MSVC 构建树 stub 在 Release/Debug 配置目录
#include "platform/agent_platform.hpp"

#include "wingman/runtime/resource_pack.hpp"
#include <spdlog/spdlog.h>
#include <cstdio>
#include <cstring>
#include <fstream>
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

bool updatePeResource(const std::string& outputPath, const std::vector<uint8_t>& resourceData) {
    std::wstring outputPathW;
    outputPathW.assign(outputPath.begin(), outputPath.end());

    // 开始资源更新
    HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
    if (!hUpdate) {
        spdlog::error("Failed to begin resource update: {}", GetLastError());
        return false;
    }

    // 完整资源数据（头部 + 负载）由平台无关的同一条代码产出：
    // PE 嵌入只是给这份字节流换个容器，Linux 上测的字节语义与此处完全一致。
    //
    // 这里不能写成 const：UpdateResourceA 的第 5 参是 LPVOID（它只读这份
    // 字节，签名不带 const 是 Win32 的历史包袱），MSVC 对 const uint8_t*
    // → LPVOID 直接 C2664。
    std::vector<uint8_t> data(resourceData);

    // 添加资源
    BOOL result = UpdateResourceA(
        hUpdate,
        RT_RCDATA,                                    // 资源类型
        MAKEINTRESOURCEA(PACK_PE_RESOURCE_ID),        // 资源 ID（与读侧同源常量）
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        data.data(),
        static_cast<DWORD>(data.size())
    );

    if (!result) {
        spdlog::error("Failed to update resource: {}", GetLastError());
        EndUpdateResource(hUpdate, TRUE);
        return false;
    }

    // 结束资源更新
    if (!EndUpdateResource(hUpdate, FALSE)) {
        spdlog::error("Failed to end resource update: {}", GetLastError());
        return false;
    }

    spdlog::info("Resource embedded successfully: {} bytes", data.size());
    return true;
}

bool replacePeIcon(const std::string& outputPath, const std::string& iconPath) {
    if (iconPath.empty()) {
        return true;  // 没有指定图标，跳过
    }

    std::wstring iconPathW, outputPathW;
    iconPathW.assign(iconPath.begin(), iconPath.end());
    outputPathW.assign(outputPath.begin(), outputPath.end());

    // 简单实现：复制图标到资源
    HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
    if (!hUpdate) {
        spdlog::warn("Failed to begin resource update for icon: {}", GetLastError());
        return false;
    }

    // 读取图标文件
    std::ifstream iconFile(iconPath, std::ios::binary);
    if (!iconFile) {
        spdlog::warn("Failed to open icon file: {}", iconPath);
        EndUpdateResource(hUpdate, TRUE);
        return false;
    }

    std::vector<uint8_t> iconData((std::istreambuf_iterator<char>(iconFile)),
                                   std::istreambuf_iterator<char>());
    iconFile.close();

    // 替换主图标 (ID = 1)
    BOOL result = UpdateResourceA(
        hUpdate,
        RT_ICON,
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        iconData.data(),
        static_cast<DWORD>(iconData.size())
    );

    if (!result) {
        spdlog::warn("Failed to update icon resource: {}", GetLastError());
        EndUpdateResource(hUpdate, TRUE);
        return false;
    }

    EndUpdateResource(hUpdate, FALSE);
    spdlog::info("Icon replaced successfully");
    return true;
}

bool setPeVersionInfo(const std::string& outputPath,
                      const std::string& appName,
                      const std::string& appVersion) {
    std::wstring outputPathW;
    outputPathW.assign(outputPath.begin(), outputPath.end());

    HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
    if (!hUpdate) {
        spdlog::warn("Failed to begin resource update for version info");
        return false;
    }

    // Parse version string (e.g. "1.2.3")
    WORD major = 1, minor = 0, patch = 0, build = 0;
    sscanf(appVersion.c_str(), "%hu.%hu.%hu.%hu", &major, &minor, &patch, &build);

    // Build VS_VERSIONINFO resource
    struct {
        VS_FIXEDFILEINFO ffi;
    } versionData = {};

    versionData.ffi.dwSignature = 0xFEEF04BD;
    versionData.ffi.dwStrucVersion = 0x00010000;
    versionData.ffi.dwFileVersionMS = MAKELONG(minor, major);
    versionData.ffi.dwFileVersionLS = MAKELONG(build, patch);
    versionData.ffi.dwProductVersionMS = MAKELONG(minor, major);
    versionData.ffi.dwProductVersionLS = MAKELONG(build, patch);
    versionData.ffi.dwFileFlagsMask = 0x3F;
    versionData.ffi.dwFileFlags = 0;
    versionData.ffi.dwFileOS = VOS_NT_WINDOWS32;
    versionData.ffi.dwFileType = VFT_APP;
    versionData.ffi.dwFileSubtype = VFT2_UNKNOWN;
    versionData.ffi.dwFileDateMS = 0;
    versionData.ffi.dwFileDateLS = 0;

    // Build full version info resource with string table
    // The resource format is complex, we build a minimal valid block
    std::wstring appNameW(appName.begin(), appName.end());
    std::wstring versionW(appVersion.begin(), appVersion.end());

    // Calculate total size needed for VS_VERSIONINFO + StringFileInfo
    size_t headerSize = sizeof(VS_FIXEDFILEINFO) + 40; // VS_VERSIONINFO header
    size_t strTableSize = 0;

    // String entries: each has key + value (wchar_t aligned)
    struct StrEntry { const wchar_t* key; const std::wstring& value; };
    StrEntry entries[] = {
        {L"ProductName", appNameW},
        {L"FileDescription", appNameW},
        {L"FileVersion", versionW},
        {L"ProductVersion", versionW},
        {L"OriginalFilename", appNameW},
        {L"CompanyName", std::wstring(L"")},
    };

    for (const auto& e : entries) {
        size_t keyLen = wcslen(e.key);
        size_t valLen = e.value.size();
        // String structure: sizeof(WORD)*6 + key wchars + padding + value wchars + padding
        strTableSize += 6 * sizeof(WORD) + keyLen * sizeof(wchar_t);
        strTableSize = (strTableSize + 3) & ~3; // align
        strTableSize += valLen * sizeof(wchar_t);
        strTableSize = (strTableSize + 3) & ~3;
    }

    size_t strTableHeaderSize = 6 * sizeof(WORD) + 8; // StringTable header + "040904b0"
    size_t strFileInfoHeaderSize = 6 * sizeof(WORD) + 14 * sizeof(wchar_t); // "StringFileInfo"
    size_t totalSize = headerSize + strFileInfoHeaderSize + strTableHeaderSize + strTableSize;
    totalSize = (totalSize + 3) & ~3;

    std::vector<uint8_t> resource(totalSize, 0);
    uint8_t* p = resource.data();

    // VS_VERSIONINFO header
    auto writeWord = [&p](WORD w) { memcpy(p, &w, sizeof(WORD)); p += sizeof(WORD); };
    auto writeDword = [&p](DWORD dw) { memcpy(p, &dw, sizeof(DWORD)); p += sizeof(DWORD); };
    auto writeWString = [&p](const wchar_t* s, size_t len) {
        memcpy(p, s, len * sizeof(wchar_t));
        p += len * sizeof(wchar_t);
    };
    auto align = [&p]() { p = (uint8_t*)(((uintptr_t)p + 3) & ~3); };

    // Write VS_FIXEDFILEINFO directly at the correct offset
    // VS_VERSIONINFO starts at offset 0
    WORD vsVersionInfoLen = (WORD)(headerSize + strFileInfoHeaderSize + strTableHeaderSize + strTableSize);
    writeWord(vsVersionInfoLen);  // wLength
    writeWord(sizeof(VS_FIXEDFILEINFO) + 40); // wValueLength
    writeWord(0);  // wType (binary)
    writeWString(L"VS_VERSION_INFO", 15);
    align();
    memcpy(p, &versionData.ffi, sizeof(VS_FIXEDFILEINFO));
    p += sizeof(VS_FIXEDFILEINFO);
    align();

    // StringFileInfo block
    size_t sfiStart = p - resource.data();
    writeWord(0); // placeholder for length
    writeWord(0); // wValueLength
    writeWord(1); // wType (text)
    writeWString(L"StringFileInfo", 14);
    align();

    // StringTable
    size_t stStart = p - resource.data();
    writeWord(0); // placeholder for length
    writeWord(0); // wValueLength
    writeWord(1); // wType (text)
    writeWString(L"040904b0", 8);
    align();

    // String entries
    for (const auto& e : entries) {
        size_t entryStart = p - resource.data();
        size_t keyLen = wcslen(e.key);
        size_t valLen = e.value.size();

        writeWord(0); // placeholder for length
        writeWord((WORD)valLen);
        writeWord(1); // wType (text)
        writeWString(e.key, keyLen);
        align();
        if (valLen > 0) writeWString(e.value.c_str(), valLen);
        align();

        // Fill in length
        size_t entryLen = (p - resource.data()) - entryStart;
        *(WORD*)(resource.data() + entryStart) = (WORD)entryLen;
    }

    // Fill in StringTable length
    *(WORD*)(resource.data() + stStart) = (WORD)((p - resource.data()) - stStart);
    // Fill in StringFileInfo length
    *(WORD*)(resource.data() + sfiStart) = (WORD)((p - resource.data()) - sfiStart);

    // Update total resource length
    *(WORD*)(resource.data()) = (WORD)(p - resource.data());

    BOOL result = UpdateResourceA(
        hUpdate,
        RT_VERSION,
        MAKEINTRESOURCEA(VS_VERSION_INFO),
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        resource.data(),
        static_cast<DWORD>(p - resource.data())
    );

    if (!result) {
        spdlog::warn("Failed to update version info resource: {}", GetLastError());
        EndUpdateResource(hUpdate, TRUE);
        return false;
    }

    if (!EndUpdateResource(hUpdate, FALSE)) {
        spdlog::warn("Failed to end version info update: {}", GetLastError());
        return false;
    }

    spdlog::info("Version info set: {} v{}", appName, appVersion);
    return true;
}

} // namespace wingman::runtime::platform
