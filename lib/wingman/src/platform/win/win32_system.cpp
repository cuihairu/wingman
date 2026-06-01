#include "wingman/system.hpp"

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <tlhelp32.h>
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
// Helper functions
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
    [[maybe_unused]] DWORD type = REG_SZ;

    if (RegQueryValueExA(hKey, value.c_str(), nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(buffer), &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return "";
    }

    RegCloseKey(hKey);
    return std::string(buffer);
}

// ============================================================================
// CPU info
// ============================================================================

CpuInfo System::getCpuInfo() {
    CpuInfo info;

    // Get CPU core count
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.cores = sysInfo.dwNumberOfProcessors;

    // Get logical processor count
    DWORD length = 0;
    GetLogicalProcessorInformation(nullptr, &length);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buffer(length);
        if (GetLogicalProcessorInformation(
                reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.data()),
                &length)) {
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

    // Read registry for CPU info
    info.vendor = readRegistryValue(
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "VendorIdentifier"
    );

    info.brand = readRegistryValue(
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "ProcessorNameString"
    );

    // Get max clock frequency
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
    static uint64_t lastIdleTime = 0, lastKernelTime = 0, lastUserTime = 0;
    static bool firstRun = true;
    static int numProcessors;

    if (firstRun) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        FILETIME ftIdle, ftKernel, ftUser;
        if (GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
            lastIdleTime = getFileTimeAsUInt64(ftIdle);
            lastKernelTime = getFileTimeAsUInt64(ftKernel);
            lastUserTime = getFileTimeAsUInt64(ftUser);
        }
        firstRun = false;
        return 0;
    }

    FILETIME ftIdle, ftKernel, ftUser;
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
        return 0;
    }

    uint64_t idleTime = getFileTimeAsUInt64(ftIdle);
    uint64_t kernelTime = getFileTimeAsUInt64(ftKernel);
    uint64_t userTime = getFileTimeAsUInt64(ftUser);

    uint64_t totalDiff = (kernelTime - lastKernelTime) + (userTime - lastUserTime);
    uint64_t idleDiff = idleTime - lastIdleTime;

    lastIdleTime = idleTime;
    lastKernelTime = kernelTime;
    lastUserTime = userTime;

    if (totalDiff == 0) return 0;

    double usage = 100.0 * (1.0 - static_cast<double>(idleDiff) / static_cast<double>(totalDiff));
    return static_cast<int>(usage);
}

int System::getCpuTemperature() {
    return -1;  // Not supported
}

// ============================================================================
// Memory info
// ============================================================================

MemoryInfo System::getMemoryInfo() {
    MemoryInfo info = {};

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
// Disk info
// ============================================================================

std::vector<DiskInfo> System::getDiskInfo() {
    std::vector<DiskInfo> result;

    DWORD drives = GetLogicalDrives();
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        if (drives & (1 << (drive - 'A'))) {
            std::string drivePath = std::string(1, drive) + ":\\";
            result.push_back(getDiskInfo(drivePath));
        }
    }

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

    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExA(drive.c_str(), &freeBytes, &totalBytes, nullptr)) {
        info.total = totalBytes.QuadPart;
        info.free = freeBytes.QuadPart;
        info.used = info.total - info.free;
        info.usage = info.total > 0 ? (static_cast<double>(info.used) / info.total * 100.0) : 0;
    }

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
// GPU info (simplified)
// ============================================================================

std::vector<GpuInfo> System::getGpuInfo() {
    std::vector<GpuInfo> result;

    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);

    for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            GpuInfo info;
            info.name = dd.DeviceName;
            result.push_back(info);
        }
    }

    return result;
}

// ============================================================================
// OS info
// ============================================================================

OsInfo System::getOsInfo() {
    OsInfo info;

    info.platform = "Windows";

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

    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        info.computerName = computerName;
    }

    char userName[UNLEN + 1];
    size = sizeof(userName);
    if (GetUserNameA(userName, &size)) {
        info.userName = userName;
    }

    return info;
}

// ============================================================================
// Network info
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

            std::ostringstream mac;
            mac << std::hex << std::setfill('0');
            for (UINT i = 0; i < adapter->AddressLength; ++i) {
                if (i > 0) mac << ":";
                mac << std::setw(2) << static_cast<int>(adapter->Address[i]);
            }
            info.macAddress = mac.str();

            info.ipAddress = adapter->IpAddressList.IpAddress.String ?
                            adapter->IpAddressList.IpAddress.String : "";
            info.subnetMask = adapter->IpAddressList.IpMask.String ?
                             adapter->IpAddressList.IpMask.String : "";

            info.isUp = (adapter->Type != MIB_IF_TYPE_LOOPBACK);
            info.bytesSent = 0;
            info.bytesReceived = 0;

            result.push_back(info);
            adapter = adapter->Next;
        }
    }

    free(adapterInfo);
    return result;
}

// ============================================================================
// Display info
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
// Other info
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
        if (tzi.StandardName[0] != L'\0') {
            int size = WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string result(size - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, &result[0], size, nullptr, nullptr);
                return result;
            }
        }
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

#endif // _WIN32
