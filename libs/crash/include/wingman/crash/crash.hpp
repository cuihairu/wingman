#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace wingman::crash {

/// Crashpad 采集配置。路径推导（handler 同目录、数据库在应用数据目录）由
/// 调用方负责，本库保持零平台宏。
struct CrashpadConfig {
    std::filesystem::path handlerPath;  ///< crashpad_handler 可执行文件
    std::filesystem::path databaseDir;  ///< dump 数据库目录（不存在会创建）
    std::string uploadUrl;              ///< 空 = 不上传（默认关，实现里另有数据库层保险）
    std::map<std::string, std::string> annotations;  ///< 写入 dump 的自描述注解
};

/// 启动 crashpad 采集（StartHandler + 数据库层面关上传）。
/// 失败返回 false，调用方降级为无采集继续运行。
bool initialize(const CrashpadConfig& config);

/// 故意空指针解引用（写页 0 触发 SIGSEGV）——仅崩溃采集验收用，不返回。
[[noreturn]] void testCrashNullPointer();

/// 枚举数据库中未处理（上一轮遗留）的崩溃报告路径。数据库不存在或查询失败
/// 返回空。
std::vector<std::filesystem::path> pendingReports(const std::filesystem::path& databaseDir);

} // namespace wingman::crash
