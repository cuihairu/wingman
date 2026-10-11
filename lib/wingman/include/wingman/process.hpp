#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wingman {

// 跨平台进程标识：Windows 侧 DWORD、POSIX 侧 pid_t，两者宽度一致，统一为
// 无符号 32 位；平台差异（有符号/错误码）收在实现层转换，见
// src/platform/win/win32_process.cpp 与 src/platform/*/posix_process.cpp。
// 约定 0 = 不存在/无结果。
using ProcessId = uint32_t;

struct ProcessInfo {
    ProcessId pid;
    std::string name;
    std::string path;

    ProcessInfo() : pid(0) {}
};

class Process {
public:
    static ProcessId find(const std::string& name);
    static std::vector<ProcessId> findAll(const std::string& name);
    static std::vector<ProcessInfo> enumerate();

    static ProcessId start(const std::string& path,
                          const std::string& args = "",
                          const std::string& workingDir = "");
    static bool wait(ProcessId pid, int timeoutMs = 0);
    static bool terminate(ProcessId pid, bool force = false);
    static bool exists(ProcessId pid);

    static std::string getName(ProcessId pid);
    static std::string getPath(ProcessId pid);
    static ProcessId getCurrentId();

    static bool waitFor(const std::string& name, int timeoutMs = 5000);
    static bool waitExit(const std::string& name, int timeoutMs = 5000);
};

} // namespace wingman
