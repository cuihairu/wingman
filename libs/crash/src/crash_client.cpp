#include "wingman/crash/crash.hpp"

#include <base/files/file_path.h>
#include <client/crash_report_database.h>
#include <client/crashpad_client.h>
#include <client/settings.h>

#include <system_error>

namespace wingman::crash {
namespace {

// std::filesystem::path::native() 在 POSIX 是 string、Windows 是 wstring，
// 与 base::FilePath 的平台构造参数一致，避免 string() 在 Windows 上的窄化
// 编码损失；反向取值用 FilePath::value()。
base::FilePath toFilePath(const std::filesystem::path& path) {
    return base::FilePath(path.native());
}

} // namespace

bool initialize(const CrashpadConfig& config) {
    std::error_code ec;
    std::filesystem::create_directories(config.databaseDir, ec);
    if (ec) {
        return false;
    }

    // 上传默认关的双保险之一：数据库设置层面显式关闭（另一道是 uploadUrl 留空
    // 不传 URL）。将来部署 collector 时由配置统一放开，两处同源。
    auto database = crashpad::CrashReportDatabase::Initialize(toFilePath(config.databaseDir));
    if (!database) {
        return false;
    }
    database->GetSettings()->SetUploadsEnabled(false);

    crashpad::CrashpadClient client;
    return client.StartHandler(
        toFilePath(config.handlerPath),
        toFilePath(config.databaseDir),
        toFilePath(config.databaseDir),  // metrics 与数据库同目录（metadata 文件）
        config.uploadUrl,
        config.annotations,
        std::vector<std::string>{},
        /*restartable=*/true,
        /*asynchronous_start=*/false);
}

void testCrashNullPointer() {
    // volatile 阻止编译器把整个空指针写优化掉；写页 0 在全平台触发 SIGSEGV/
    // 访问违例。验收断言见 scripts/verify-crashpad.sh。
    volatile int* nullAddress = nullptr;
    *nullAddress = 0x2a;
    __builtin_unreachable();
}

std::vector<std::filesystem::path> pendingReports(const std::filesystem::path& databaseDir) {
    std::vector<std::filesystem::path> paths;
    // 只读枚举：数据库不存在（首轮运行）不算错误，返回空
    auto database =
        crashpad::CrashReportDatabase::InitializeWithoutCreating(toFilePath(databaseDir));
    if (!database) {
        return paths;
    }
    std::vector<crashpad::CrashReportDatabase::Report> reports;
    if (database->GetPendingReports(&reports) != crashpad::CrashReportDatabase::kNoError) {
        return paths;
    }
    paths.reserve(reports.size());
    for (const auto& report : reports) {
        paths.emplace_back(report.file_path.value());
    }
    return paths;
}

} // namespace wingman::crash
