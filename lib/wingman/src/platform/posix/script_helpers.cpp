#include "platform/script_helpers.hpp"

#include <cstdlib>
#include <sys/stat.h>

namespace wingman::platform {

std::string readEnvironmentVariable(const std::string& key) {
    if (const char* val = std::getenv(key.c_str())) {
        return val;
    }
    return {};
}

uint64_t readFileModifiedTime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
#if defined(__APPLE__)
        return static_cast<uint64_t>(st.st_mtimespec.tv_sec) * 1000000000ULL +
               static_cast<uint64_t>(st.st_mtimespec.tv_nsec);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        return static_cast<uint64_t>(st.st_mtim.tv_sec) * 1000000000ULL +
               static_cast<uint64_t>(st.st_mtim.tv_nsec);
#else
        return static_cast<uint64_t>(st.st_mtime) * 1000000000ULL;
#endif
    }
    return 0;
}

} // namespace wingman::platform
