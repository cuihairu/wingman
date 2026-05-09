#include <iostream>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <wingman/version.hpp>

// 服务名称
const wchar_t* SERVICE_NAME = L"WingmanAgent";

// 进程管理函数
bool IsProcessRunning(const wchar_t* processName) {
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

// 启动服务
bool StartService() {
    // 检查是否已经在运行
    if (IsProcessRunning(L"wingman-client.exe")) {
        std::cout << "Wingman service is already running." << std::endl;
        return true;
    }

    // 启动客户端进程
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    std::wstring cmdLine = L"wingman-client.exe";

    if (!CreateProcessW(
        nullptr,                   // 模块名
        const_cast<wchar_t*>(cmdLine.c_str()), // 命令行
        nullptr,                   // 进程安全属性
        nullptr,                   // 线程安全属性
        FALSE,                      // 不继承句柄
        0,                         // 创建标志
        nullptr,                   // 环境变量
        L".",                      // 当前目录
        &si, &pi
    )) {
        std::cerr << "Failed to start service: " << GetLastError() << std::endl;
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::cout << "Wingman service started." << std::endl;
    return true;
}

// 停止服务
bool StopService() {
    // 查找并终止进程
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create process snapshot." << std::endl;
        return false;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, L"wingman-client.exe") == 0) {
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
        std::cout << "Wingman service stopped." << std::endl;
        return true;
    } else {
        std::cout << "Wingman service is not running." << std::endl;
        return true;
    }
}

// 查看状态
bool StatusService() {
    if (IsProcessRunning(L"wingman-client.exe")) {
        std::cout << "Wingman service status: RUNNING" << std::endl;
        return true;
    } else {
        std::cout << "Wingman service status: STOPPED" << std::endl;
        return true;
    }
}

// 运行脚本
bool RunScript(const std::string& scriptPath) {
    // 检查脚本文件是否存在
    FILE* f = nullptr;
    if (fopen_s(&f, scriptPath.c_str(), "r") != 0 || !f) {
        std::cerr << "Script file not found: " << scriptPath << std::endl;
        return false;
    }
    fclose(f);

    std::cout << "Running script: " << scriptPath << std::endl;

    // 使用 Lua 解释器运行脚本
    std::string luaCmd = "luajit.exe " + scriptPath;
    int result = system(luaCmd.c_str());

    return result == 0;
}

// 显示帮助
void ShowHelp() {
    std::cout << "Wingman CLI Tool " << WINGMAN_VERSION << "\n\n"
              << "Usage: wingman-cli <command> [options]\n\n"
              << "Commands:\n"
              << "  start              Start Wingman service\n"
              << "  stop               Stop Wingman service\n"
              << "  status             Show service status\n"
              << "  script <file.lua>  Run Lua script\n"
              << "  help               Show this help message\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // 初始化日志
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::default_logger()->sinks().push_back(consoleSink);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    if (argc < 2) {
        ShowHelp();
        return 1;
    }

    std::string command = argv[1];

    if (command == "start") {
        return StartService() ? 0 : 1;
    } else if (command == "stop") {
        return StopService() ? 0 : 1;
    } else if (command == "status") {
        return StatusService() ? 0 : 1;
    } else if (command == "script") {
        if (argc < 3) {
            std::cerr << "Usage: wingman-cli script <file.lua>" << std::endl;
            return 1;
        }
        return RunScript(argv[2]) ? 0 : 1;
    } else if (command == "help" || command == "--help" || command == "-h") {
        ShowHelp();
        return 0;
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        ShowHelp();
        return 1;
    }
}
