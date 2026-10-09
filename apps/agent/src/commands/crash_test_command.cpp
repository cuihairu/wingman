#include "wingman/runtime/commands/crash_test_command.hpp"

#include <spdlog/spdlog.h>

#ifdef WINGMAN_HAS_CRASHPAD
#include "wingman/crash/crash.hpp"
#endif

namespace wingman::runtime::commands {

int crashTestCommand() {
#ifdef WINGMAN_HAS_CRASHPAD
    spdlog::warn("crash-test: intentionally dereferencing null pointer to verify crash capture");
    // 落盘 sink 在崩溃瞬间不再有机会冲刷，先显式冲日志
    spdlog::default_logger()->flush();
    crash::testCrashNullPointer();  // [[noreturn]]
#else
    spdlog::error("crash-test: this build has no Crashpad support (WINGMAN_ENABLE_CRASHPAD=OFF)");
#endif
    return 1;
}

} // namespace wingman::runtime::commands
