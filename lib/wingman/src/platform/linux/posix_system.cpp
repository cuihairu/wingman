#include "wingman/system.hpp"

#if defined(__linux__) || defined(__APPLE__)

#include <fstream>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(__linux__)
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <mach/mach.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace wingman {

// ============================================================================
// Helper functions
// ============================================================================

static std::string readFile(const std::string& path) {
#if defined(__linux__)
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
#else
    return "";
#endif
}

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

    // Remove trailing newline
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

#if defined(__linux__)
    // Read /proc/cpuinfo
    std::string cpuinfo = readFile("/proc/cpuinfo");
    std::istringstream stream(cpuinfo);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("vendor_id") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                info.vendor = line.substr(pos + 2);
            }
        } else if (line.find("model name") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                info.brand = line.substr(pos + 2);
            }
        } else if (line.find("cpu cores") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                info.cores = std::stoi(line.substr(pos + 2));
            }
        } else if (line.find("siblings") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                info.threads = std::stoi(line.substr(pos + 2));
            }
        } else if (line.find("cpu MHz") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                info.maxClock = static_cast<int>(std::stod(line.substr(pos + 2)));
                info.currentClock = info.maxClock;
            }
        }
    }

    info.usage = getCpuUsage();
    info.temperature = getCpuTemperature();

#elif defined(__APPLE__)
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
#endif

    return info;
}

int System::getCpuUsage() {
#if defined(__linux__)
    static uint64_t lastTotal = 0, lastIdle = 0;
    static bool firstRun = true;

    std::string stat = readFile("/proc/stat");
    std::istringstream stream(stat);
    std::string cpu;
    uint64_t user, nice, system, idle;

    stream >> cpu >> user >> nice >> system >> idle;

    uint64_t total = user + nice + system + idle;

    if (firstRun) {
        lastTotal = total;
        lastIdle = idle;
        firstRun = false;
        return 0;
    }

    uint64_t totalDiff = total - lastTotal;
    uint64_t idleDiff = idle - lastIdle;

    lastTotal = total;
    lastIdle = idle;

    if (totalDiff == 0) return 0;

    return static_cast<int>(100.0 * (1.0 - static_cast<double>(idleDiff) / static_cast<double>(totalDiff)));

#elif defined(__APPLE__)
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
#endif

    return 0;
}

int System::getCpuTemperature() {
    // Not supported yet
    return -1;
}

// ============================================================================
// Memory info
// ============================================================================

MemoryInfo System::getMemoryInfo() {
    MemoryInfo info = {};

#if defined(__linux__)
    std::string meminfo = readFile("/proc/meminfo");
    std::istringstream stream(meminfo);
    std::string line;

    uint64_t total = 0, available = 0;

    while (std::getline(stream, line)) {
        if (line.find("MemTotal:") == 0) {
            size_t pos = line.find(':');
            std::string value = line.substr(pos + 1);
            total = std::stoull(value) * 1024;  // kB -> bytes
        } else if (line.find("MemAvailable:") == 0) {
            size_t pos = line.find(':');
            std::string value = line.substr(pos + 1);
            available = std::stoull(value) * 1024;  // kB -> bytes
        }
    }

    info.total = total;
    info.available = available;
    info.used = total - available;
    info.usage = total > 0 ? (static_cast<double>(info.used) / total * 100.0) : 0;

#elif defined(__APPLE__)
    int mib[2];
    int64_t physicalMemory;
    size_t length;

    // Total memory
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(int64_t);
    sysctl(mib, 2, &physicalMemory, &length, nullptr, 0);
    info.total = physicalMemory;

    // Available memory
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
#endif

    return info;
}

// ============================================================================
// Disk info
// ============================================================================

std::vector<DiskInfo> System::getDiskInfo() {
    std::vector<DiskInfo> result;

#if defined(__linux__)
    // Read /proc/mounts to get mount points
    std::string mounts = readFile("/proc/mounts");
    std::istringstream stream(mounts);
    std::string line;

    while (std::getline(stream, line)) {
        std::istringstream lineStream(line);
        std::string device, mountPoint, fsType;

        lineStream >> device >> mountPoint >> fsType;

        // Only check real devices
        if (device.find("/dev/") == 0 || device.find("/srv/") == 0) {
            DiskInfo info = getDiskInfo(mountPoint);
            if (!info.fileSystem.empty()) {
                result.push_back(info);
            }
        }
    }

#elif defined(__APPLE__)
    struct statfs *mounts;
    int count = getmntinfo(&mounts, MNT_WAIT);

    for (int i = 0; i < count; i++) {
        std::string mountPoint = mounts[i].f_mntonname;
        DiskInfo info = getDiskInfo(mountPoint);
        if (info.total > 0) {
            result.push_back(info);
        }
    }
#endif

    return result;
}

DiskInfo System::getDiskInfo(const std::string& drive) {
    DiskInfo info;
    info.drive = drive;

#if defined(__linux__)
    struct statvfs stat;
    if (statvfs(drive.c_str(), &stat) == 0) {
        info.total = stat.f_blocks * stat.f_frsize;
        info.free = stat.f_bavail * stat.f_frsize;
        info.used = info.total - info.free;
        info.usage = info.total > 0 ? (static_cast<double>(info.used) / info.total * 100.0) : 0;
    }

    // Read filesystem type
    std::string mounts = readFile("/proc/mounts");
    std::istringstream stream(mounts);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find(drive) != std::string::npos) {
            std::istringstream lineStream(line);
            std::string device, mountPoint, fsType;
            lineStream >> device >> mountPoint >> fsType;
            if (mountPoint == drive) {
                info.fileSystem = fsType;
                break;
            }
        }
    }

#elif defined(__APPLE__)
    struct statfs stat;
    if (statfs(drive.c_str(), &stat) == 0) {
        info.total = stat.f_blocks * stat.f_bsize;
        info.free = stat.f_bavail * stat.f_bsize;
        info.used = info.total - info.free;
        info.usage = info.total > 0 ? (static_cast<double>(info.used) / info.total * 100.0) : 0;
        info.fileSystem = stat.f_fstypename;
        info.volumeName = stat.f_mntfromname;
    }
#endif

    return info;
}

// ============================================================================
// GPU info (name only)
// ============================================================================

std::vector<GpuInfo> System::getGpuInfo() {
    std::vector<GpuInfo> result;

#if defined(__linux__)
    // Read GPU info from /sys/class/drm
    std::string cardPath = "/sys/class/drm";
    DIR* dir = opendir(cardPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find("card") == 0 && name.find("-") == std::string::npos) {
                std::string devicePath = cardPath + "/" + name + "/device";
                std::string ueventPath = devicePath + "/uevent";

                std::string uevent = readFile(ueventPath);
                std::istringstream stream(uevent);
                std::string line;

                while (std::getline(stream, line)) {
                    if (line.find("PCI_SLOT_NAME") == 0 || line.find("PCI_ID") == 0) {
                        size_t pos = line.find('=');
                        if (pos != std::string::npos) {
                            GpuInfo gpuInfo;
                            gpuInfo.name = line.substr(pos + 1);
                            result.push_back(gpuInfo);
                            break;
                        }
                    }
                }
            }
        }
        closedir(dir);
    }

    // Try to read more detailed GPU name from /sys/bus/pci/devices
    DIR* pciDir = opendir("/sys/bus/pci/devices");
    if (pciDir) {
        struct dirent* entry;
        while ((entry = readdir(pciDir)) != nullptr) {
            std::string devicePath = "/sys/bus/pci/devices/" + std::string(entry->d_name);
            std::string classPath = devicePath + "/class";

            std::string classCode = readFile(classPath);
            // VGA controller class code is 0x03
            if (classCode.find("0x03") != std::string::npos) {
                std::string ueventPath = devicePath + "/uevent";
                std::string uevent = readFile(ueventPath);

                std::istringstream stream(uevent);
                std::string line;

                while (std::getline(stream, line)) {
                    if (line.find("PCI_ID") == 0) {
                        size_t pos = line.find('=');
                        if (pos != std::string::npos) {
                            std::string pciId = line.substr(pos + 1);

                            // Try to read vendor and device names
                            std::string vendorPath = devicePath + "/vendor";
                            std::string devicePath2 = devicePath + "/device";

                            std::string vendor = readFile(vendorPath);
                            std::string device = readFile(devicePath2);

                            GpuInfo gpuInfo;
                            gpuInfo.name = "GPU " + pciId;  // Simplified version
                            result.push_back(gpuInfo);
                        }
                    }
                }
            }
        }
        closedir(pciDir);
    }

#elif defined(__APPLE__)
    // macOS uses IOKit to get GPU info (simplified)
    // Use system_profiler command
    std::string output = getCommandOutput("system_profiler SPDisplaysDataType 2>/dev/null | grep 'Chipset Model' | cut -d: -f2 | sed 's/^[ ]*//'");

    if (!output.empty()) {
        GpuInfo gpuInfo;
        gpuInfo.name = output;
        result.push_back(gpuInfo);
    }
#endif

    return result;
}

// ============================================================================
// OS info
// ============================================================================

OsInfo System::getOsInfo() {
    OsInfo info;

#if defined(__linux__)
    info.platform = "Linux";

    // Read /etc/os-release
    std::string osRelease = readFile("/etc/os-release");
    std::istringstream stream(osRelease);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("PRETTY_NAME=") == 0) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string value = line.substr(pos + 1);
                // Remove quotes
                if (value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                info.version = value;
            }
        }
    }

    // Get kernel version
    struct utsname unameInfo;
    if (uname(&unameInfo) == 0) {
        info.build = unameInfo.release;
        info.architecture = unameInfo.machine;
        info.computerName = unameInfo.nodename;
    }

#elif defined(__APPLE__)
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
#endif

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

        // Check if already exists
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

#if defined(__linux__)
    // Use xrandr command to get display info
    std::string output = getCommandOutput("xrandr 2>/dev/null");
    if (!output.empty()) {
        std::istringstream stream(output);
        std::string line;
        int index = 0;

        while (std::getline(stream, line)) {
            if (line.find(" connected") != std::string::npos) {
                DisplayInfo info;
                info.index = index++;

                size_t pos = line.find(' ');
                info.name = line.substr(0, pos);

                // Parse resolution
                size_t resPos = line.find('x');
                if (resPos != std::string::npos) {
                    size_t spacePos = line.rfind(' ', resPos - 1);
                    if (spacePos != std::string::npos) {
                        info.width = std::stoi(line.substr(spacePos + 1, resPos - spacePos - 1));
                    }
                    size_t endPos = line.find(' ', resPos + 1);
                    if (endPos != std::string::npos) {
                        info.height = std::stoi(line.substr(resPos + 1, endPos - resPos - 1));
                    }
                }

                info.isPrimary = (line.find("primary") != std::string::npos);
                result.push_back(info);
            }
        }
    }

#elif defined(__APPLE__)
    // macOS uses CGDisplay API
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
#endif

    return result;
}

// ============================================================================
// Other info
// ============================================================================

int System::getUptime() {
#if defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<int>(info.uptime);
    }

#elif defined(__APPLE__)
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};

    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        time_t now = time(nullptr);
        return static_cast<int>(now - boottime.tv_sec);
    }
#endif

    return 0;
}

std::string System::getDateTime() {
    time_t now = time(nullptr);
    char buffer[80];
    struct tm timeinfo;

#if defined(__APPLE__)
    localtime_r(&now, &timeinfo);
#else
    localtime_r(&now, &timeinfo);
#endif

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buffer);
}

std::string System::getTimeZone() {
    time_t now = time(nullptr);
    struct tm timeinfo;

#if defined(__APPLE__)
    localtime_r(&now, &timeinfo);
#else
    localtime_r(&now, &timeinfo);
#endif

    return timeinfo.tm_zone;
}

int System::getProcessCount() {
#if defined(__linux__)
    int count = 0;
    DIR* procDir = opendir("/proc");
    if (procDir) {
        struct dirent* entry;
        while ((entry = readdir(procDir)) != nullptr) {
            if (isdigit(entry->d_name[0])) {
                count++;
            }
        }
        closedir(procDir);
    }
    return count;

#elif defined(__APPLE__)
    int pidCount = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    return pidCount;
#endif

    return 0;
}

int System::getThreadCount() {
    // Sum threads across all processes from /proc/[pid]/stat
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        std::string name = entry.path().filename().string();
        if (std::all_of(name.begin(), name.end(), ::isdigit)) {
            std::ifstream ifs(entry.path() / "stat");
            std::string line;
            if (std::getline(ifs, line)) {
                // Format: pid (comm) state ... num_threads ...
                // Find closing ')' then skip 17 fields to num_threads
                auto pos = line.rfind(')');
                if (pos != std::string::npos) {
                    std::istringstream iss(line.substr(pos + 2));
                    std::string field;
                    for (int i = 0; i < 17 && iss >> field; ++i) {}
                    int threads = 0;
                    if (iss >> threads) count += threads;
                }
            }
        }
    }
    return count;
}

} // namespace wingman

#endif // __linux__ || __APPLE__
