// POSIX 实现（Linux/macOS）：uname
#include "platform/host_info.hpp"

#include <sys/utsname.h>
#include <unistd.h>

namespace wingman::runtime::platform {

std::string hostname() {
    struct utsname uts;
    if (uname(&uts) == 0) {
        return std::string(uts.nodename);
    }
    return "unix-pc";
}

} // namespace wingman::runtime::platform
