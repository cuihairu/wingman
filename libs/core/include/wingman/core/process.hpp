#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace wingman {

using ProcessHandle = HANDLE;
using ProcessId = DWORD;

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
