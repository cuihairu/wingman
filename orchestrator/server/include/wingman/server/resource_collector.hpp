#pragma once

#include "wingman/server/protocol.hpp"
#include <string>
#include <cstdint>

namespace wingman::server {

// 资源收集器 - 收集本地系统资源信息
class ResourceCollector {
public:
    ResourceCollector() = default;
    ~ResourceCollector() = default;

    // 收集所有资源信息
    ResourceStats collect() const;

    // ========== 单项收集 ==========

    // CPU 信息
    struct CpuInfo {
        double usage = 0.0;
        int cores = 0;
        std::string model;
    };
    CpuInfo collectCpu() const;

    // 内存信息
    struct MemoryInfo {
        uint64_t total = 0;
        uint64_t available = 0;
        double usage = 0.0;
    };
    MemoryInfo collectMemory() const;

    // 硬盘信息
    struct DiskInfo {
        uint64_t total = 0;
        uint64_t available = 0;
        double usage = 0.0;
    };
    DiskInfo collectDisk(const std::string& path = "C:\\") const;

    // 网络信息
    struct NetworkInfo {
        uint64_t upSpeed = 0;
        uint64_t downSpeed = 0;
        std::string localIp;
        std::string publicIp;
    };
    NetworkInfo collectNetwork() const;

    // 系统信息
    struct SystemInfo {
        double temperature = 0.0;
        std::string os;
        std::string arch;
        std::string hostname;
    };
    SystemInfo collectSystem() const;
};

} // namespace wingman::server
