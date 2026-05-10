#include "wingman/runtime/commands/stop_command.hpp"
#include <spdlog/spdlog.h>
#include <windows.h>
#include <tlhelp32.h>

namespace wingman::runtime::commands {

namespace {

bool isProcessRunning(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return false;
    }

    bool found = false;
    do {
        if (_wcsicmp(pe32.szExeFile, processName) == 0) {
            found = true;
            break;
        }
    } while (Process32NextW(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return found;
}

} // anonymous namespace

int stopCommand() {
    // 查找并终止 wingman-agent 进程
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create process snapshot");
        return 1;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            // 匹配 wingman-runtime.exe 或 wingman-agent.exe
            if (_wcsicmp(pe32.szExeFile, L"wingman-runtime.exe") == 0 ||
                _wcsicmp(pe32.szExeFile, L"wingman-agent.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    found = true;
                }
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (found) {
        spdlog::info("Wingman agent stopped");
        return 0;
    } else {
        spdlog::warn("Wingman agent is not running");
        return 0;
    }
}

int statusCommand() {
    bool running = isProcessRunning(L"wingman-runtime.exe") ||
                   isProcessRunning(L"wingman-agent.exe");

    if (running) {
        spdlog::info("Wingman agent status: RUNNING");
        return 0;
    } else {
        spdlog::info("Wingman agent status: STOPPED");
        return 1;
    }
}

} // namespace wingman::runtime::commands
