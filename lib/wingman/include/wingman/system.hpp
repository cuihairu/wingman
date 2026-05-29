#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wingman {

// CPU information
struct CpuInfo {
    std::string vendor;
    std::string brand;
    int cores;
    int threads;
    int maxClock;      // MHz
    int currentClock;  // MHz
    double usage;      // 0-100%
    int temperature;   // Celsius (if supported)
};

// Memory information
struct MemoryInfo {
    uint64_t total;      // Bytes
    uint64_t available;  // Bytes
    uint64_t used;       // Bytes
    double usage;        // 0-100%
};

// Disk information
struct DiskInfo {
    std::string drive;      // e.g. "C:\\" or "/"
    uint64_t total;         // Bytes
    uint64_t free;          // Bytes
    uint64_t used;          // Bytes
    double usage;           // 0-100%
    std::string fileSystem; // e.g. NTFS, ext4
    std::string volumeName;
};

// GPU information (simplified, name only)
struct GpuInfo {
    std::string name;       // GPU name
};

// OS information
struct OsInfo {
    std::string platform;      // Windows / Linux / macOS
    std::string version;       // e.g. 10.0.19041
    std::string build;         // Build number
    std::string architecture;  // x64 / ARM64
    std::string computerName;
    std::string userName;
};

// Network information
struct NetworkAdapter {
    std::string name;
    std::string description;
    std::string macAddress;
    std::string ipAddress;
    std::string subnetMask;
    bool isUp;
    uint64_t bytesSent;
    uint64_t bytesReceived;
};

// Monitor information
struct DisplayInfo {
    int index;
    std::string name;
    int width;
    int height;
    int refreshRate;
    bool isPrimary;
};

// System information class
class System {
public:
    // === CPU ===
    static CpuInfo getCpuInfo();
    static int getCpuUsage();
    static int getCpuTemperature();

    // === Memory ===
    static MemoryInfo getMemoryInfo();

    // === Disk ===
    static std::vector<DiskInfo> getDiskInfo();
    static DiskInfo getDiskInfo(const std::string& drive);

    // === GPU ===
    static std::vector<GpuInfo> getGpuInfo();

    // === OS ===
    static OsInfo getOsInfo();

    // === Network ===
    static std::vector<NetworkAdapter> getNetworkAdapters();

    // === Display ===
    static std::vector<DisplayInfo> getDisplayInfo();

    // === Other ===
    static int getUptime();  // System uptime (seconds)
    static std::string getDateTime();
    static std::string getTimeZone();

    // === Process/Thread ===
    static int getProcessCount();
    static int getThreadCount();

private:
#if defined(_WIN32)
    // Windows helper function
    static std::string readRegistryValue(const std::string& path, const std::string& value);
    static uint64_t getFileTimeAsUInt64(const FILETIME& ft);
#endif
};

} // namespace wingman
