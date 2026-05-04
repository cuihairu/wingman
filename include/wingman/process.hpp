#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
using ProcessHandle = HANDLE;
using ProcessId = DWORD;
#else
#include <unistd.h>
using ProcessHandle = pid_t;
using ProcessId = pid_t;
#endif

namespace wingman {

struct ProcessInfo {
    ProcessId pid;
    std::string name;
    std::string path;

    ProcessInfo() : pid(0) {}
};

class Process {
public:
    // === 查找进程 ===

    // 按名称查找进程（精确匹配）
    static ProcessId find(const std::string& name);

    // 查找所有匹配名称的进程
    static std::vector<ProcessId> findAll(const std::string& name);

    // 获取所有进程列表
    static std::vector<ProcessInfo> enumerate();

    // === 进程操作 ===

    // 启动进程
    static ProcessId start(const std::string& path,
                          const std::string& args = "",
                          const std::string& workingDir = "");

    // 等待进程结束
    static bool wait(ProcessId pid, int timeoutMs = 0);

    // 终止进程
    static bool terminate(ProcessId pid, bool force = false);

    // 检查进程是否存在
    static bool exists(ProcessId pid);

    // === 进程信息 ===

    // 获取进程名称
    static std::string getName(ProcessId pid);

    // 获取进程路径
    static std::string getPath(ProcessId pid);

    // 获取当前进程ID
    static ProcessId getCurrentId();

    // === 工具函数 ===

    // 等待进程出现
    static bool waitFor(const std::string& name, int timeoutMs = 5000);

    // 等待进程退出
    static bool waitExit(const std::string& name, int timeoutMs = 5000);

private:
#ifdef _WIN32
    // 枚举进程回调
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
#endif
};

} // namespace wingman
