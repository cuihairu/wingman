#include "wingman/server/resource_collector.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <comdef.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <hostname/hostname.h>
#endif

#include <chrono>
#include <fstream>
#include <sstream>
#include <regex>

namespace wingman::server {

ResourceStats ResourceCollector::collect() const {
    ResourceStats stats;

    auto cpu = collectCpu();
    stats.cpuUsage = cpu.usage;
    stats.cpuCores = cpu.cores;
    stats.cpuModel = cpu.model;

    auto memory = collectMemory();
    stats.totalMemory = memory.total;
    stats.availableMemory = memory.available;
    stats.memoryUsage = memory.usage;

    auto disk = collectDisk();
    stats.totalDisk = disk.total;
    stats.availableDisk = disk.available;
    stats.diskUsage = disk.usage;

    auto network = collectNetwork();
    stats.networkUp = network.upSpeed;
    stats.networkDown = network.downSpeed;
    stats.localIp = network.localIp;
    stats.publicIp = network.publicIp;

    auto system = collectSystem();
    stats.temperature = system.temperature;
    stats.os = system.os;
    stats.arch = system.arch;

    stats.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    return stats;
}

// ========== Windows 实现 ==========
#ifdef _WIN32

CpuInfo ResourceCollector::collectCpu() const {
    CpuInfo info;

    // 获取 CPU 核心数
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.cores = sysInfo.dwNumberOfProcessors;

    // 获取 CPU 型号
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char cpuName[256] = {0};
        DWORD size = sizeof(cpuName);
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
            (LPBYTE)cpuName, &size);
        info.model = cpuName;
        RegCloseKey(hKey);
    }

    // 获取 CPU 使用率
    PDH_STATUS status;
    HQUERY hQuery = NULL;
    HCOUNTER hCounter = NULL;

    status = PdhOpenQuery(NULL, 0, &hQuery);
    if (status == ERROR_SUCCESS) {
        status = PdhAddEnglishCounter(hQuery,
            "\\Processor(_Total)\\% Processor Time",
            0, &hCounter);
        if (status == ERROR_SUCCESS) {
            PdhCollectQueryData(hQuery);
            Sleep(100);  // 等待采样
            PdhCollectQueryData(hQuery);

            PDH_FMT_COUNTERVALUE counterVal;
            status = PdhGetFormattedCounterValue(hCounter,
                PDH_FMT_DOUBLE, NULL, &counterVal);
            if (status == ERROR_SUCCESS) {
                info.usage = counterVal.doubleValue;
            }
        }
        PdhCloseQuery(hQuery);
    }

    return info;
}

MemoryInfo ResourceCollector::collectMemory() const {
    MemoryInfo info;

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);

    info.total = memInfo.ullTotalPhys;
    info.available = memInfo.ullAvailPhys;
    info.usage = (double)(memInfo.dwMemoryLoad);

    return info;
}

DiskInfo ResourceCollector::collectDisk(const std::string& path) const {
    DiskInfo info;

    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExA(path.c_str(), &freeBytes, &totalBytes, NULL)) {
        info.total = totalBytes.QuadPart;
        info.available = freeBytes.QuadPart;
        info.usage = 100.0 * (1.0 - (double)freeBytes.QuadPart / totalBytes.QuadPart);
    }

    return info;
}

NetworkInfo ResourceCollector::collectNetwork() const {
    NetworkInfo info;

    // 获取本地 IP
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (getaddrinfo(hostname, nullptr, &hints, &res) == 0) {
            char ip[INET_ADDRSTRLEN] = {0};
            if (getnameinfo(res->ai_addr, res->ai_addrlen, ip, sizeof(ip),
                nullptr, 0, NI_NUMERICHOST) == 0) {
                info.localIp = ip;
            }
            freeaddrinfo(res);
        }
    }

    // 公网 IP 需要（这里简化处理，实际需要通过外部 API）
    // 可以在心跳中由客户端主动获取

    return info;
}

SystemInfo ResourceCollector::collectSystem() const {
    SystemInfo info;

    // 主机名
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info.hostname = hostname;
    }

    // 操作系统版本
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    #pragma warning(push)
    #pragma warning(disable: 4996)
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        std::ostringstream oss;
        oss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
        info.os = oss.str();
    }
    #pragma warning(pop)

    // 架构
    info.arch = "x64";

    return info;
}

#else
// ========== Linux 实现 ==========

CpuInfo ResourceCollector::collectCpu() const {
    CpuInfo info;

    // 读取 CPU 核心数
    info.cores = sysconf(_SC_NPROCESSORS_ONLN);

    // 读取 CPU 使用率
    std::ifstream stat("/proc/stat");
    if (stat.is_open()) {
        std::string line;
        std::getline(stat, line);
        std::istringstream iss(line);
        std::string cpu;
        long user, nice, system, idle;
        iss >> cpu >> user >> nice >> system >> idle;
        long total = user + nice + system + idle;
        if (total > 0) {
            info.usage = 100.0 * (1.0 - (double)idle / total);
        }
    }

    // 读取 CPU 型号
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") == 0) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    info.model = line.substr(pos + 2);
                    break;
                }
            }
        }
    }

    return info;
}

MemoryInfo ResourceCollector::collectMemory() const {
    MemoryInfo info;

    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        info.total = sys_info.totalram * sys_info.mem_unit;
        info.available = sys_info.freeram * sys_info.mem_unit;
        info.usage = 100.0 * (1.0 - (double)sys_info.freeram / sys_info.totalram);
    }

    return info;
}

DiskInfo ResourceCollector::collectDisk(const std::string& path) const {
    DiskInfo info;

    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) == 0) {
        info.total = stat.f_blocks * stat.f_frsize;
        info.available = stat.f_bavail * stat.f_frsize;
        info.usage = 100.0 * (1.0 - (double)stat.f_bavail / stat.f_blocks);
    }

    return info;
}

NetworkInfo ResourceCollector::collectNetwork() const {
    NetworkInfo info;

    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info.localIp = hostname;
    }

    return info;
}

SystemInfo ResourceCollector::collectSystem() const {
    SystemInfo info;

    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info.hostname = hostname;
    }

    info.os = "Linux";
    info.arch = "x64";

    return info;
}

#endif

} // namespace wingman::server
