-- Wingman 系统信息示例
-- 演示如何获取系统和硬件信息

local wingman = require("wingman")

print("=== Wingman 系统信息 ===\n")

-- CPU 信息
print("--- CPU 信息 ---")
local cpu = wingman.system.getCpuInfo()
print(string.format("  厂商: %s", cpu.vendor))
print(string.format("  型号: %s", cpu.brand))
print(string.format("  核心: %d", cpu.cores))
print(string.format("  线程: %d", cpu.threads))
print(string.format("  最大频率: %d MHz", cpu.maxClock))
print(string.format("  当前频率: %d MHz", cpu.currentClock))
print(string.format("  使用率: %.1f%%", cpu.usage))

-- 内存信息
print("\n--- 内存信息 ---")
local mem = wingman.system.getMemoryInfo()
print(string.format("  总计: %.2f GB", mem.total / 1024 / 1024 / 1024))
print(string.format("  可用: %.2f GB", mem.available / 1024 / 1024 / 1024))
print(string.format("  已用: %.2f GB", mem.used / 1024 / 1024 / 1024))
print(string.format("  使用率: %.1f%%", mem.usage))

-- 磁盘信息
print("\n--- 磁盘信息 ---")
local disks = wingman.system.getDiskInfo()
for i, disk in ipairs(disks) do
    print(string.format("  [%s] %s", disk.drive, disk.volumeName))
    print(string.format("    总计: %.2f GB", disk.total / 1024 / 1024 / 1024))
    print(string.format("    可用: %.2f GB", disk.free / 1024 / 1024 / 1024))
    print(string.format("    文件系统: %s", disk.fileSystem))
end

-- GPU 信息
print("\n--- GPU 信息 ---")
local gpus = wingman.system.getGpuInfo()
for i, gpu in ipairs(gpus) do
    print(string.format("  [%d] %s", i, gpu.name))
    print(string.format("    专用内存: %.2f GB", gpu.dedicatedMemory / 1024 / 1024 / 1024))
    print(string.format("    共享内存: %.2f GB", gpu.sharedMemory / 1024 / 1024 / 1024))
end

-- 操作系统信息
print("\n--- 操作系统信息 ---")
local os = wingman.system.getOsInfo()
print(string.format("  平台: %s", os.platform))
print(string.format("  版本: %s", os.version))
print(string.format("  构建号: %s", os.build))
print(string.format("  架构: %s", os.architecture))
print(string.format("  计算机名: %s", os.computerName))
print(string.format("  用户名: %s", os.userName))

-- 网络适配器
print("\n--- 网络适配器 ---")
local adapters = wingman.system.getNetworkAdapters()
for i, adapter in ipairs(adapters) do
    print(string.format("  [%d] %s", i, adapter.description))
    print(string.format("    MAC: %s", adapter.macAddress))
    print(string.format("    IP: %s", adapter.ipAddress))
    print(string.format("    状态: %s", adapter.isUp and "UP" or "DOWN"))
end

-- 显示器信息
print("\n--- 显示器信息 ---")
local displays = wingman.system.getDisplayInfo()
for i, display in ipairs(displays) do
    print(string.format("  [%d] %s", i, display.name))
    print(string.format("    分辨率: %dx%d", display.width, display.height))
    print(string.format("    刷新率: %d Hz", display.refreshRate))
    print(string.format("    主显示器: %s", display.isPrimary and "是" or "否"))
end

-- 其他信息
print("\n--- 其他信息 ---")
print(string.format("  系统运行时间: %d 秒", wingman.system.getUptime()))
print(string.format("  当前时间: %s", wingman.system.getDateTime()))
print(string.format("  进程数: %d", wingman.system.getProcessCount()))
print(string.format("  线程数: %d", wingman.system.getThreadCount()))

print("\n=== 完成 ===")
