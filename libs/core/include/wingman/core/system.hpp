#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

namespace wingman {

// CPU 信息
struct CpuInfo {
    std::string vendor;
    std::string brand;
    int cores;
    int threads;
    int maxClock;      // MHz
    int currentClock;  // MHz
    double usage;      // 0-100%
    int temperature;   // 摄氏度 (如果支持)
};

// 内存信息
struct MemoryInfo {
    uint64_t total;      // 字节
    uint64_t available;  // 字节
    uint64_t used;       // 字节
    double usage;        // 0-100%
};

// 磁盘信息
struct DiskInfo {
    std::string drive;      // 如 "C:\\"
    uint64_t total;         // 字节
    uint64_t free;          // 字节
    uint64_t used;          // 字节
    double usage;           // 0-100%
    std::string fileSystem; // 如 NTFS
    std::string volumeName;
};

// GPU 信息
struct GpuInfo {
    std::string name;
    uint64_t dedicatedMemory;  // 字节
    uint64_t sharedMemory;     // 字节
    double usage;              // 0-100%
    int temperature;           // 摄氏度
};

// 操作系统信息
struct OsInfo {
    std::string platform;      // Windows
    std::string version;       // 如 10.0.19041
    std::string build;         // 构建号
    std::string architecture;  // x64 / ARM64
    std::string computerName;
    std::string userName;
};

// 网络信息
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

// 显示器信息
struct DisplayInfo {
    int index;
    std::string name;
    int width;
    int height;
    int refreshRate;
    bool isPrimary;
};

// 系统信息类
class System {
public:
    // === CPU ===
    static CpuInfo getCpuInfo();
    static int getCpuUsage();
    static int getCpuTemperature();

    // === 内存 ===
    static MemoryInfo getMemoryInfo();

    // === 磁盘 ===
    static std::vector<DiskInfo> getDiskInfo();
    static DiskInfo getDiskInfo(const std::string& drive);

    // === GPU ===
    static std::vector<GpuInfo> getGpuInfo();

    // === 操作系统 ===
    static OsInfo getOsInfo();

    // === 网络 ===
    static std::vector<NetworkAdapter> getNetworkAdapters();

    // === 显示器 ===
    static std::vector<DisplayInfo> getDisplayInfo();

    // === 其他 ===
    static int getUptime();  // 系统运行时间（秒）
    static std::string getDateTime();
    static std::string getTimeZone();

    // === 进程/线程 ===
    static int getProcessCount();
    static int getThreadCount();

private:
    // 辅助函数
    static std::string readRegistryValue(const std::string& path, const std::string& value);
    static uint64_t getFileTimeAsUInt64(const FILETIME& ft);
};

} // namespace wingman
