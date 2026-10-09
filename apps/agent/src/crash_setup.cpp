#include "wingman/runtime/crash_setup.hpp"

#include <spdlog/spdlog.h>

#ifdef WINGMAN_HAS_CRASHPAD
#include "wingman/crash/crash.hpp"
#include "wingman/platform/app_paths.hpp"
#include "wingman/version.hpp"

#include <filesystem>
#endif

namespace wingman::runtime {

void setupCrashReporting(const char* argv0) {
#ifdef WINGMAN_HAS_CRASHPAD
    namespace fs = std::filesystem;

    crash::CrashpadConfig config;
    // handler 与本可执行文件同目录分发（构建期 POST_BUILD copy 到产物目录）；
    // PATH 直呼时 argv[0] 不含目录，退化为当前工作目录解析，由 StartHandler
    // 报错并走降级路径。
    config.handlerPath =
        (argv0 && *argv0 ? fs::absolute(argv0).parent_path() : fs::path("."))
        / "crashpad_handler";
    config.databaseDir = platform::appDataDir() / "crashes";
    config.annotations = {
        {"version", WINGMAN_VERSION_FULL},
        {"platform", platform::platformName()},
        {"argv0", argv0 ? argv0 : ""},
    };

    if (crash::initialize(config)) {
        spdlog::info("Crash reporting enabled: handler={}, database={}",
                     config.handlerPath.string(), config.databaseDir.string());
    } else {
        spdlog::warn("Crash reporting disabled: initialization failed (handler={}, database={})",
                     config.handlerPath.string(), config.databaseDir.string());
    }

    // 把「上次崩了」带进常规日志流：启动即枚举上一轮遗留报告
    for (const auto& report : crash::pendingReports(config.databaseDir)) {
        spdlog::warn("Pending crash report from previous run: {}", report.string());
    }
#endif
}

} // namespace wingman::runtime
