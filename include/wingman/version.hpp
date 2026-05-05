#pragma once

#include <string>

namespace wingman {

// Version information
// Automatically updated by build system
namespace version {
    // Base version from CMakeLists.txt
    constexpr const char* const MAJOR = "0";
    constexpr const char* const MINOR = "1";
    constexpr const char* const PATCH = "0";

    // Version suffix (e.g., "nightly", "beta", "rc1")
    // Set by CMake with -DWINGMAN_VERSION_SUFFIX
    #ifndef WINGMAN_VERSION_SUFFIX
    #define WINGMAN_VERSION_SUFFIX ""
    #endif

    inline const std::string& getSuffix() {
        static const std::string suffix = WINGMAN_VERSION_SUFFIX;
        return suffix;
    }

    // Full version string (e.g., "0.1.0" or "0.1.0-nightly")
    inline std::string getFullVersion() {
        std::string version = std::string(MAJOR) + "." + MINOR + "." + PATCH;
        const std::string& suffix = getSuffix();
        if (!suffix.empty()) {
            version += "-" + suffix;
        }
        return version;
    }

    // Build information
    inline const char* getBuildDate() {
        return __DATE__;
    }

    inline const char* getBuildTime() {
        return __TIME__;
    }

    inline const char* getCompiler() {
        #if defined(_MSC_VER)
        return "MSVC";
        #elif defined(__GNUC__)
        return "GCC " __VERSION__;
        #elif defined(__clang__)
        return "Clang " __VERSION__;
        #else
        return "Unknown";
        #endif
    }

    inline std::string getCompilerVersion() {
        #if defined(_MSC_FULL_VER)
        #define STR2(x) #x
        #define STR(x) STR2(x)
        return "MSVC " STR(_MSC_FULL_VER);
        #undef STR
        #undef STR2
        #elif defined(__GNUC__)
        return "GCC " __VERSION__;
        #elif defined(__clang__)
        return "Clang " __VERSION__;
        #else
        return "Unknown";
        #endif
    }
} // namespace version

} // namespace wingman
