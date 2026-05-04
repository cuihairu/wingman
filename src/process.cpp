#include "wingman/process.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <chrono>
#include <thread>

namespace wingman {

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

} // namespace wingman
