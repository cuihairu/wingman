#include "platform/script_helpers.hpp"

#include <Windows.h>

namespace wingman::platform {

std::string readEnvironmentVariable(const std::string& key) {
    DWORD needed = GetEnvironmentVariableA(key.c_str(), nullptr, 0);
    if (needed > 0) {
        std::string buf(needed - 1, '\0');
        GetEnvironmentVariableA(key.c_str(), buf.data(), needed);
        return buf;
    }
    return {};
}

uint64_t readFileModifiedTime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        LARGE_INTEGER time;
        time.HighPart = data.ftLastWriteTime.dwHighDateTime;
        time.LowPart = data.ftLastWriteTime.dwLowDateTime;
        return static_cast<uint64_t>(time.QuadPart / 10000 - 11644473600000LL);
    }
    return 0;
}

} // namespace wingman::platform
