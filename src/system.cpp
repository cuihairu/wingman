#include "wingman/system.hpp"

#include <windows.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")

#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace wingman {

// ============================================================================
// 辅助函数
// ============================================================================

uint64_t System::getFileTimeAsUInt64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) |
           static_cast<uint64_t>(ft.dwLowDateTime);
}

std::string System::readRegistryValue(const std::string& path, const std::string& value) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "";
    }

    char buffer[256];
    DWORD size = sizeof(buffer);
    DWORD type = REG_SZ;

    if (RegQueryValueExA(hKey, value.c_str(), nullptr, &type,
                        reinterpret_cast<LPBYTE>(buffer), &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return "";
    }

    RegCloseKey(hKey);
    return std::string(buffer);
}

// ============================================================================
// CPU 信息
// ============================================================================

CpuInfo System::getCpuInfo() {
    CpuInfo info;

    // 获取 CPU 核心数
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.cores = sysInfo.dwNumberOfProcessors;

    // 获取逻辑处理器数
    DWORD length = 0;
    GetLogicalProcessorInformation(nullptr, &length);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buffer(length);
        if (GetLogicalProcessorInformation(buffer.data(), &length)) {
            int count = 0;
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr =
                reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.data());
            for (size_t i = 0; i < length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
                if (ptr[i].Relationship == RelationProcessorCore) {
                    count++;
                }
            }
            info.threads = sysInfo.dwNumberOfProcessors;
        }
    } else {
        info.threads = info.cores;
    }

    // 读取注册表获取 CPU 信息
    info.vendor = readRegistryValue(
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "VendorIdentifier"
    );

    info.brand = readRegistryValue(
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "ProcessorNameString"
    );

    // 获取最大时钟频率
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD freq = 0;
        DWORD size = sizeof(freq);
        RegQueryValueExA(hKey, "~MHz", nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(&freq), &size);
        info.maxClock = static_cast<int>(freq);
        RegCloseKey(hKey);
    }

    info.currentClock = info.maxClock;
    info.usage = getCpuUsage();
    info.temperature = getCpuTemperature();

    return info;
}

int System::getCpuUsage() {
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static bool firstRun = true;
    static int numProcessors;

    if (firstRun) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        FILETIME ftIdle, ftKernel, ftUser;
        if (GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
            lastCPU = getFileTimeAsUInt64(ftKernel) + getFileTimeAsUInt64(ftUser);
            lastSysCPU = getFileTimeAsUInt64(ftKernel);
            lastUserCPU = getFileTimeAsUInt64(ftUser);
        }
        firstRun = false;
        return 0;
    }

    FILETIME ftIdle, ftKernel, ftUser;
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
        return 0;
    }

    ULARGE_INTEGER sysCPU = getFileTimeAsUInt64(ftKernel);
    ULARGE_INTEGER userCPU = getFileTimeAsUInt64(ftUser);

    ULARGE_INTEGER totalCPU = sysCPU + userCPU;
    ULARGE_INTEGER totalDiff = totalCPU.QuadPart - lastCPU.QuadPart;

    lastCPU = totalCPU;
    lastSysCPU = sysCPU;
    lastUserCPU = userCPU;

    if (totalDiff == 0) return 0;

    return static_cast<int>(100.0 - (static_cast<double>(totalDiff) / numProcessors * 100.0));
}

int System::getCpuTemperature() {
    // 需要安装 Open Hardware Monitor 或类似的软件来读取温度
    // Windows 没有直接读取 CPU 温度的 API
    // 这里返回 -1 表示不支持
    return -1;
}

// ============================================================================
// 内存信息
// ============================================================================

MemoryInfo System::getMemoryInfo() {
    MemoryInfo info;

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        info.total = status.ullTotalPhys;
        info.available = status.ullAvailPhys;
        info.used = info.total - info.available;
        info.usage = status.dwMemoryLoad;
    }

    return info;
}

// ============================================================================
// 磁盘信息
// ============================================================================

std::vector<DiskInfo> System::getDiskInfo() {
    std::vector<DiskInfo> result;

    // 获取所有逻辑驱动器
    DWORD drives = GetLogicalDrives();
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        if (drives & (1 << (drive - 'A'))) {
            std::string drivePath = std::string(1, drive) + ":\\";
            result.push_back(getDiskInfo(drivePath));
        }
    }

    // 移除空驱动器
    result.erase(
        std::remove_if(result.begin(), result.end(),
            [](const DiskInfo& d) { return d.total == 0; }),
        result.end()
    );

    return result;
}

DiskInfo System::getDiskInfo(const std::string& drive) {
    DiskInfo info;
    info.drive = drive;

    // 获取磁盘空间
    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExA(drive.c_str(), &freeBytes, &totalBytes, nullptr)) {
        info.total = totalBytes.QuadPart;
        info.free = freeBytes.QuadPart;
        info.used = info.total - info.free;
        info.usage = info.total > 0 ? (static_cast<double>(info.used) / info.total * 100.0) : 0;
    }

    // 获取文件系统
    char fileSystem[MAX_PATH];
    char volumeName[MAX_PATH];
    DWORD maxComponentLength, fileSystemFlags;
    if (GetVolumeInformationA(drive.c_str(), volumeName, MAX_PATH,
                             nullptr, &maxComponentLength, &fileSystemFlags,
                             fileSystem, MAX_PATH)) {
        info.fileSystem = fileSystem;
        info.volumeName = volumeName;
    }

    return info;
}

// ============================================================================
// GPU 信息
// ============================================================================

std::vector<GpuInfo> System::getGpuInfo() {
    std::vector<GpuInfo> result;

    // 使用 WMI 或 DirectX 可以获取更详细的 GPU 信息
    // 这里提供基础实现
    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);

    for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            GpuInfo info;
            info.name = dd.DeviceName;
            info.dedicatedMemory = 0;
            info.sharedMemory = 0;
            info.usage = 0;
            info.temperature = -1;
            result.push_back(info);
        }
    }

    return result;
}

// ============================================================================
// 操作系统信息
// ============================================================================

OsInfo System::getOsInfo() {
    OsInfo info;

    info.platform = "Windows";

    // 获取版本信息
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                     "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD size = sizeof(buffer);

        if (RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS) {
            info.build = buffer;
        }

        size = sizeof(buffer);
        if (RegQueryValueExA(hKey, "DisplayVersion", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS) {
            info.version = buffer;
        }

        RegCloseKey(hKey);
    }

    // 获取架构
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info.architecture = "x64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info.architecture = "ARM64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            info.architecture = "x86";
            break;
        default:
            info.architecture = "Unknown";
            break;
    }

    // 获取计算机名
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        info.computerName = computerName;
    }

    // 获取用户名
    char userName[UNLEN + 1];
    size = sizeof(userName);
    if (GetUserNameA(userName, &size)) {
        info.userName = userName;
    }

    return info;
}

// ============================================================================
// 网络信息
// ============================================================================

std::vector<NetworkAdapter> System::getNetworkAdapters() {
    std::vector<NetworkAdapter> result;

    PIP_ADAPTER_INFO adapterInfo;
    ULONG bufferSize = sizeof(IP_ADAPTER_INFO);

    adapterInfo = reinterpret_cast<PIP_ADAPTER_INFO>(malloc(bufferSize));
    if (GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = reinterpret_cast<PIP_ADAPTER_INFO>(malloc(bufferSize));
    }

    if (GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO adapter = adapterInfo;
        while (adapter) {
            NetworkAdapter info;
            info.name = adapter->AdapterName;
            info.description = adapter->Description;

            // MAC 地址
            std::ostringstream mac;
            mac << std::hex << std::setfill('0');
            for (UINT i = 0; i < adapter->AddressLength; ++i) {
                if (i > 0) mac << ":";
                mac << std::setw(2) << static_cast<int>(adapter->Address[i]);
            }
            info.macAddress = mac.str();

            // IP 地址
            info.ipAddress = adapter->IpAddressList.IpAddress.S_un.S_addr ?
                            adapter->IpAddressList.IpAddress.S_un.S_addr : "";
            info.subnetMask = adapter->IpAddressList.IpMask.S_un.S_addr ?
                             adapter->IpAddressList.IpMask.S_un.S_addr : "";

            info.isUp = (adapter->Type != MIB_IF_TYPE_LOOPBACK);

            info.bytesSent = adapter->OutOctets;
            info.bytesReceived = adapter->InOctets;

            result.push_back(info);
            adapter = adapter->Next;
        }
    }

    free(adapterInfo);
    return result;
}

// ============================================================================
// 显示器信息
// ============================================================================

std::vector<DisplayInfo> System::getDisplayInfo() {
    std::vector<DisplayInfo> result;

    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);

    for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i) {
        DisplayInfo info;
        info.index = static_cast<int>(i);
        info.name = dd.DeviceName;
        info.isPrimary = (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;

        // 获取显示器的当前设置
        DEVMODE dm;
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
            info.width = dm.dmPelsWidth;
            info.height = dm.dmPelsHeight;
            info.refreshRate = dm.dmDisplayFrequency;
        }

        result.push_back(info);
    }

    return result;
}

// ============================================================================
// 其他信息
// ============================================================================

int System::getUptime() {
    return static_cast<int>(GetTickCount64() / 1000);
}

std::string System::getDateTime() {
    time_t now = time(nullptr);
    char buffer[80];
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buffer);
}

std::string System::getTimeZone() {
    TIME_ZONE_INFORMATION tzi;
    if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        wchar_t name[64];
        // 转换时区名称
        std::wstring wname(tzi.StandardName);
        return std::string(wname.begin(), wname.end());
    }
    return "";
}

int System::getProcessCount() {
    DWORD count = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(pe32);
        if (Process32First(snapshot, &pe32)) {
            do {
                count++;
            } while (Process32Next(snapshot, &pe32));
        }
        CloseHandle(snapshot);
    }
    return static_cast<int>(count);
}

int System::getThreadCount() {
    // 这只是估算，实际需要遍历所有进程
    DWORD threads = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(pe32);
        if (Process32First(snapshot, &pe32)) {
            do {
                threads += pe32.cntThreads;
            } while (Process32Next(snapshot, &pe32));
        }
        CloseHandle(snapshot);
    }
    return static_cast<int>(threads);
}

} // namespace wingman
