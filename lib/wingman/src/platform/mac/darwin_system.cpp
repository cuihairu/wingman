#include "wingman/system.hpp"

#if defined(__APPLE__)

#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

namespace wingman {

// ============================================================================
// Helper functions
// ============================================================================

static std::string getCommandOutput(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}

// ============================================================================
// CPU info
// ============================================================================

CpuInfo System::getCpuInfo() {
    CpuInfo info;

    // Get CPU core count
    int physicalCores = 0;
    size_t len = sizeof(physicalCores);
    sysctlbyname("hw.physicalcpu", &physicalCores, &len, nullptr, 0);
    info.cores = physicalCores;

    int logicalCores = 0;
    len = sizeof(logicalCores);
    sysctlbyname("hw.logicalcpu", &logicalCores, &len, nullptr, 0);
    info.threads = logicalCores;

    // Get CPU frequency
    uint64_t freq = 0;
    len = sizeof(freq);
    sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0);
    info.maxClock = static_cast<int>(freq / 1000000);  // Hz -> MHz
    info.currentClock = info.maxClock;

    // Get CPU brand
    char brand[128];
    len = sizeof(brand);
    sysctlbyname("machdep.cpu.brand_string", brand, &len, nullptr, 0);
    info.brand = brand;

    info.vendor = "Apple";

    info.usage = getCpuUsage();
    info.temperature = getCpuTemperature();

    return info;
}

int System::getCpuUsage() {
    static host_cpu_load_info_data_t lastLoadInfo = {0};
    static bool firstRun = true;

    host_cpu_load_info_data_t loadInfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                       (host_info_t)&loadInfo, &count) != KERN_SUCCESS) {
        return 0;
    }

    if (firstRun) {
        lastLoadInfo = loadInfo;
        firstRun = false;
        return 0;
    }

    uint64_t totalDiff = (loadInfo.cpu_ticks[CPU_STATE_USER] - lastLoadInfo.cpu_ticks[CPU_STATE_USER]) +
                         (loadInfo.cpu_ticks[CPU_STATE_SYSTEM] - lastLoadInfo.cpu_ticks[CPU_STATE_SYSTEM]) +
                         (loadInfo.cpu_ticks[CPU_STATE_IDLE] - lastLoadInfo.cpu_ticks[CPU_STATE_IDLE]) +
                         (loadInfo.cpu_ticks[CPU_STATE_NICE] - lastLoadInfo.cpu_ticks[CPU_STATE_NICE]);

    uint64_t idleDiff = loadInfo.cpu_ticks[CPU_STATE_IDLE] - lastLoadInfo.cpu_ticks[CPU_STATE_IDLE];

    lastLoadInfo = loadInfo;

    if (totalDiff == 0) return 0;

    return static_cast<int>(100.0 * (1.0 - static_cast<double>(idleDiff) / static_cast<double>(totalDiff)));
}

int System::getCpuTemperature() {
    return -1;
}

// ============================================================================
// Memory info
// ============================================================================

MemoryInfo System::getMemoryInfo() {
    MemoryInfo info = {};

    int mib[2];
    int64_t physicalMemory;
    size_t length;

    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(int64_t);
    sysctl(mib, 2, &physicalMemory, &length, nullptr, 0);
    info.total = physicalMemory;

    vm_size_t pageSize;
    host_page_size(mach_host_self(), &pageSize);

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO,
                         (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
        uint64_t used = (vmStats.active_count + vmStats.wire_count) * pageSize;
        info.used = used;
        info.available = info.total - used;
        info.usage = static_cast<double>(used) / info.total * 100.0;
    }

    return info;
}

// ============================================================================
// Disk info
// ============================================================================

std::vector<DiskInfo> System::getDiskInfo() {
    std::vector<DiskInfo> result;

    struct statfs *mounts;
    int count = getmntinfo(&mounts, MNT_WAIT);

    for (int i = 0; i < count; i++) {
        std::string mountPoint = mounts[i].f_mntonname;
        DiskInfo info = getDiskInfo(mountPoint);
        if (info.total > 0) {
            result.push_back(info);
        }
    }

    return result;
}

DiskInfo System::getDiskInfo(const std::string& drive) {
    DiskInfo info;
    info.drive = drive;

    struct statfs stat;
    if (statfs(drive.c_str(), &stat) == 0) {
        info.total = stat.f_blocks * stat.f_bsize;
        info.free = stat.f_bavail * stat.f_bsize;
        info.used = info.total - info.free;
        info.usage = info.total > 0 ? (static_cast<double>(info.used) / info.total * 100.0) : 0;
        info.fileSystem = stat.f_fstypename;
        info.volumeName = stat.f_mntfromname;
    }

    return info;
}

// ============================================================================
// GPU info (name only)
// ============================================================================

std::vector<GpuInfo> System::getGpuInfo() {
    std::vector<GpuInfo> result;

    // macOS uses system_profiler command to get GPU info
    std::string output = getCommandOutput("system_profiler SPDisplaysDataType 2>/dev/null | grep 'Chipset Model' | cut -d: -f2 | sed 's/^[ ]*//'");

    if (!output.empty()) {
        GpuInfo gpuInfo;
        gpuInfo.name = output;
        result.push_back(gpuInfo);
    }

    return result;
}

// ============================================================================
// OS info
// ============================================================================

OsInfo System::getOsInfo() {
    OsInfo info;

    info.platform = "macOS";

    // Get macOS version
    char swVersion[32];
    size_t len = sizeof(swVersion);
    sysctlbyname("kern.osproductversion", swVersion, &len, nullptr, 0);
    info.version = "macOS " + std::string(swVersion);

    // Get build version
    char buildVersion[32];
    len = sizeof(buildVersion);
    sysctlbyname("kern.osversion", buildVersion, &len, nullptr, 0);
    info.build = buildVersion;

    // Get architecture
    struct utsname unameInfo;
    if (uname(&unameInfo) == 0) {
        info.architecture = unameInfo.machine;
        info.computerName = unameInfo.nodename;
    }

    // Get username
    char userName[256];
    if (getlogin_r(userName, sizeof(userName)) == 0) {
        info.userName = userName;
    }

    return info;
}

// ============================================================================
// Network info
// ============================================================================

std::vector<NetworkAdapter> System::getNetworkAdapters() {
    std::vector<NetworkAdapter> result;

    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        return result;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        int family = ifa->ifa_addr->sa_family;

        NetworkAdapter info;
        info.name = ifa->ifa_name;
        info.isUp = (ifa->ifa_flags & IFF_UP) != 0;

        if (family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
            info.ipAddress = ip;
        } else if (family == AF_INET6) {
            char ip[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr, ip, sizeof(ip));
            info.ipAddress = ip;
        }

        bool found = false;
        for (auto& existing : result) {
            if (existing.name == info.name) {
                if (!info.ipAddress.empty() && existing.ipAddress.empty()) {
                    existing.ipAddress = info.ipAddress;
                }
                found = true;
                break;
            }
        }

        if (!found && info.isUp) {
            info.bytesSent = 0;
            info.bytesReceived = 0;
            result.push_back(info);
        }
    }

    freeifaddrs(ifaddr);

    return result;
}

// ============================================================================
// Display info
// ============================================================================

std::vector<DisplayInfo> System::getDisplayInfo() {
    std::vector<DisplayInfo> result;

    CGDirectDisplayID displays[16];
    uint32_t count;
    if (CGGetOnlineDisplayList(16, displays, &count) == kCGErrorSuccess) {
        for (uint32_t i = 0; i < count; i++) {
            DisplayInfo info;
            info.index = static_cast<int>(i);
            info.isPrimary = (CGDisplayPrimaryDisplay(displays[i]) == displays[i]);

            CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displays[i]);
            if (mode) {
                info.width = static_cast<int>(CGDisplayModeGetWidth(mode));
                info.height = static_cast<int>(CGDisplayModeGetHeight(mode));
                info.refreshRate = static_cast<int>(CGDisplayModeGetRefreshRate(mode));
                CGDisplayModeRelease(mode);
            }

            info.name = "Display " + std::to_string(i);
            result.push_back(info);
        }
    }

    return result;
}

// ============================================================================
// Other info
// ============================================================================

int System::getUptime() {
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};

    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        time_t now = time(nullptr);
        return static_cast<int>(now - boottime.tv_sec);
    }

    return 0;
}

std::string System::getDateTime() {
    time_t now = time(nullptr);
    char buffer[80];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buffer);
}

std::string System::getTimeZone() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_zone;
}

int System::getProcessCount() {
    int pidCount = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    return pidCount;
}

int System::getThreadCount() {
    return 0;
}

} // namespace wingman

#endif // __APPLE__
