#include "wingman/process.hpp"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fstream>
#endif

#include <chrono>
#include <thread>

namespace wingman {

// ============================================================================
// Windows 实现
// ============================================================================

#ifdef _WIN32

ProcessId Process::find(const std::string& name) {
    auto results = findAll(name);
    return results.empty() ? 0 : results[0];
}

std::vector<ProcessId> Process::findAll(const std::string& name) {
    std::vector<ProcessId> results;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return results;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(snapshot, &pe32)) {
        CloseHandle(snapshot);
        return results;
    }

    do {
        std::string processName = pe32.szExeFile;
        if (processName == name || processName.find(name) != std::string::npos) {
            results.push_back(pe32.th32ProcessID);
        }
    } while (Process32Next(snapshot, &pe32));

    CloseHandle(snapshot);
    return results;
}

std::vector<ProcessInfo> Process::enumerate() {
    std::vector<ProcessInfo> results;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return results;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(snapshot, &pe32)) {
        CloseHandle(snapshot);
        return results;
    }

    do {
        ProcessInfo info;
        info.pid = pe32.th32ProcessID;
        info.name = pe32.szExeFile;

        // 获取完整路径
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            char path[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, NULL, path, MAX_PATH)) {
                info.path = path;
            }
            CloseHandle(hProcess);
        }

        results.push_back(info);
    } while (Process32Next(snapshot, &pe32));

    CloseHandle(snapshot);
    return results;
}

ProcessId Process::start(const std::string& path,
                         const std::string& args,
                         const std::string& workingDir) {
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    std::string command = path;
    if (!args.empty()) {
        command += " " + args;
    }

    BOOL result = CreateProcessA(
        nullptr,
        const_cast<char*>(command.c_str()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &si,
        &pi
    );

    if (!result) {
        return 0;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return pi.dwProcessId;
}

bool Process::wait(ProcessId pid, int timeoutMs) {
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!hProcess) {
        return false;
    }

    DWORD result = WaitForSingleObject(
        hProcess,
        timeoutMs > 0 ? timeoutMs : INFINITE
    );

    CloseHandle(hProcess);
    return result == WAIT_OBJECT_0;
}

bool Process::terminate(ProcessId pid, bool force) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        return false;
    }

    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result != 0;
}

bool Process::exists(ProcessId pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return false;
    }

    DWORD exitCode;
    BOOL result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    return result && (exitCode == STILL_ACTIVE);
}

std::string Process::getName(ProcessId pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return "";
    }

    char name[MAX_PATH];
    if (!GetModuleFileNameExA(hProcess, NULL, name, MAX_PATH)) {
        CloseHandle(hProcess);
        return "";
    }

    CloseHandle(hProcess);

    // 只返回文件名
    std::string path(name);
    size_t pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

std::string Process::getPath(ProcessId pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return "";
    }

    char path[MAX_PATH];
    if (!GetModuleFileNameExA(hProcess, NULL, path, MAX_PATH)) {
        CloseHandle(hProcess);
        return "";
    }

    CloseHandle(hProcess);
    return std::string(path);
}

ProcessId Process::getCurrentId() {
    return GetCurrentProcessId();
}

bool Process::waitFor(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(name) != 0) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Process::waitExit(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(name) == 0) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// Linux 实现
// ============================================================================

#else

ProcessId Process::find(const std::string& name) {
    auto results = findAll(name);
    return results.empty() ? 0 : results[0];
}

std::vector<ProcessId> Process::findAll(const std::string& name) {
    std::vector<ProcessId> results;

    DIR* proc = opendir("/proc");
    if (!proc) {
        return results;
    }

    struct dirent* entry;
    while ((entry = readdir(proc)) != nullptr) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        ProcessId pid = atoi(entry->d_name);
        if (pid == 0) {
            continue;
        }

        std::string procName = getName(pid);
        if (procName == name || procName.find(name) != std::string::npos) {
            results.push_back(pid);
        }
    }

    closedir(proc);
    return results;
}

std::vector<ProcessInfo> Process::enumerate() {
    std::vector<ProcessInfo> results;

    DIR* proc = opendir("/proc");
    if (!proc) {
        return results;
    }

    struct dirent* entry;
    while ((entry = readdir(proc)) != nullptr) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        ProcessId pid = atoi(entry->d_name);
        if (pid == 0) {
            continue;
        }

        ProcessInfo info;
        info.pid = pid;
        info.name = getName(pid);
        info.path = getPath(pid);
        results.push_back(info);
    }

    closedir(proc);
    return results;
}

ProcessId Process::start(const std::string& path,
                         const std::string& args,
                         const std::string& workingDir) {
    ProcessId pid = fork();
    if (pid == 0) {
        // 子进程
        if (!workingDir.empty()) {
            chdir(workingDir.c_str());
        }

        execlp(path.c_str(), path.c_str(), args.c_str(), nullptr);
        exit(1);  // 如果 exec 失败
    } else if (pid > 0) {
        // 父进程
        return pid;
    }

    return 0;  // fork 失败
}

bool Process::wait(ProcessId pid, int timeoutMs) {
    if (timeoutMs == 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status);
    }

    auto start = std::chrono::steady_clock::now();
    while (true) {
        int status;
        ProcessId result = waitpid(pid, &status, WNOHANG);

        if (result == pid) {
            return true;
        }

        if (!exists(pid)) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Process::terminate(ProcessId pid, bool force) {
    int result = force ? SIGKILL : SIGTERM;
    return kill(pid, result) == 0;
}

bool Process::exists(ProcessId pid) {
    return kill(pid, 0) == 0;
}

std::string Process::getName(ProcessId pid) {
    std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");
    if (!comm.is_open()) {
        return "";
    }

    std::string name;
    std::getline(comm, name);

    // 移除换行符
    if (!name.empty() && name.back() == '\n') {
        name.pop_back();
    }

    return name;
}

std::string Process::getPath(ProcessId pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/exe";
    char buffer[PATH_MAX];
    ssize_t len = readlink(path.c_str(), buffer, sizeof(buffer) - 1);

    if (len == -1) {
        return "";
    }

    buffer[len] = '\0';
    return std::string(buffer);
}

ProcessId Process::getCurrentId() {
    return getpid();
}

bool Process::waitFor(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(name) != 0) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Process::waitExit(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(name) == 0) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#endif

} // namespace wingman
