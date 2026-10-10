// Windows 实现：GetComputerNameA
#include "platform/host_info.hpp"

#include <windows.h>

namespace wingman::runtime::platform {

std::string hostname() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }
    return "windows-pc";
}

} // namespace wingman::runtime::platform
