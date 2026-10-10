#include "wingman/platform/app_paths.hpp"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace wingman::platform {

std::filesystem::path appDataDir() {
    std::filesystem::path base;
#ifdef _WIN32
    // 崩溃转储等机器级数据取 LocalAppData（Roaming 是跨机漫游场景）
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        base = std::filesystem::path(path) / "wingman";
        CoTaskMemFree(path);
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        base = std::filesystem::path(home) / "Library" / "Application Support" / "wingman";
    }
#else
    // XDG 基目录规范：XDG_DATA_HOME 须为绝对路径，否则按未设置处理
    std::filesystem::path dataHome;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        std::filesystem::path candidate(xdg);
        if (candidate.is_absolute()) {
            dataHome = candidate;
        }
    }
    if (dataHome.empty()) {
        if (const char* home = std::getenv("HOME")) {
            dataHome = std::filesystem::path(home) / ".local" / "share";
        }
    }
    if (!dataHome.empty()) {
        base = dataHome / "wingman";
    }
#endif

    if (base.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    if (ec) {
        return {};
    }
    return base;
}

std::filesystem::path roamingAppDataDir() {
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        std::filesystem::path base(path);
        CoTaskMemFree(path);
        return base;
    }
    return {};
#else
    // 非 Windows 保持历史口径（db 模块脚本数据目录基座，不走 XDG 变量）
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share";
    }
    return {};
#endif
}

std::filesystem::path executableDir() {
#ifdef _WIN32
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        return std::filesystem::path(exePath).parent_path();
    }
    return {};
#else
    // 非 Windows 保持历史口径（game_profile 原实现用 CWD）
    return std::filesystem::current_path();
#endif
}

std::string platformName() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

} // namespace wingman::platform
