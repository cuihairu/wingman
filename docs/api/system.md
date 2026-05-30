# System API

`wingman.system` 提供系统信息查询功能，包括 CPU、内存、磁盘、GPU 和操作系统信息。

## CPU 信息

:::tabs

== Python

```python
from wingman import system

# 获取 CPU 信息
cpu = system.get_cpu_info()
print(f"处理器: {cpu['brand']}")
print(f"核心数: {cpu['cores']}")
print(f"线程数: {cpu['threads']}")
print(f"使用率: {cpu['usage']}%")

# 仅获取使用率
usage = system.get_cpu_usage()
print(f"CPU 使用率: {usage}%")
```

== Lua

```lua
local system = require("wingman.system")

-- 获取 CPU 信息
local cpu = system.getCpuInfo()
print("处理器: " .. cpu.brand)
print("核心数: " .. cpu.cores)
print("线程数: " .. cpu.threads)
print("使用率: " .. cpu.usage .. "%")

-- 仅获取使用率
local usage = system.getCpuUsage()
print("CPU 使用率: " .. usage .. "%")
```

:::

## 内存信息

:::tabs

== Python

```python
from wingman import system

# 获取内存信息
mem = system.get_memory_info()
print(f"总内存: {mem['total'] / 1024 / 1024 / 1024:.2f} GB")
print(f"可用内存: {mem['available'] / 1024 / 1024 / 1024:.2f} GB")
print(f"使用率: {mem['usage']:.1f}%")
```

== Lua

```lua
local system = require("wingman.system")

-- 获取内存信息
local mem = system.getMemoryInfo()
print("总内存: " .. string.format("%.2f", mem.total / 1024 / 1024 / 1024) .. " GB")
print("可用内存: " .. string.format("%.2f", mem.available / 1024 / 1024 / 1024) .. " GB")
print("使用率: " .. string.format("%.1f", mem.usage) .. "%")
```

:::

## 磁盘信息

:::tabs

== Python

```python
from wingman import system

# 获取所有磁盘信息
disks = system.get_disk_info()
for disk in disks:
    print(f"{disk['drive']}: {disk['used'] / 1024 / 1024 / 1024:.1f}GB / {disk['total'] / 1024 / 1024 / 1024:.1f}GB")

# 获取指定磁盘信息
c_drive = system.get_disk_info("C:")
print(f"C 盘使用率: {c_drive['usage']:.1f}%")
```

== Lua

```lua
local system = require("wingman.system")

-- 获取所有磁盘信息
local disks = system.getDiskInfo()
for i, disk in ipairs(disks) do
    print(disk.drive .. ": " .. string.format("%.1f", disk.used / 1024 / 1024 / 1024) ..
          "GB / " .. string.format("%.1f", disk.total / 1024 / 1024 / 1024) .. "GB")
end

-- 获取指定磁盘信息
local cDrive = system.getDiskInfo("C:")
print("C 盘使用率: " .. string.format("%.1f", cDrive.usage) .. "%")
```

:::

## GPU 信息

:::tabs

== Python

```python
from wingman import system

# 获取 GPU 信息
gpu = system.get_gpu_info()
print(f"显卡: {gpu['name']}")
```

== Lua

```lua
local system = require("wingman.system")

-- 获取 GPU 信息
local gpu = system.getGpuInfo()
print("显卡: " .. gpu.name)
```

:::

## 操作系统信息

:::tabs

== Python

```python
from wingman import system

# 获取系统信息
os_info = system.get_os_info()
print(f"系统: {os_info['platform']} {os_info['version']}")
print(f"架构: {os_info['architecture']}")
print(f"计算机名: {os_info['computerName']}")
print(f"用户名: {os_info['userName']}")
```

== Lua

```lua
local system = require("wingman.system")

-- 获取系统信息
local osInfo = system.getOsInfo()
print("系统: " .. osInfo.platform .. " " .. osInfo.version)
print("架构: " .. osInfo.architecture)
print("计算机名: " .. osInfo.computerName)
print("用户名: " .. osInfo.userName)
```

:::

---

## 完整示例

### 系统监控脚本

:::tabs

== Python

```python
from wingman import system
import time

def print_system_status():
    """打印系统状态"""
    # CPU
    cpu = system.get_cpu_info()
    print(f"CPU: {cpu['brand']}")
    print(f"  使用率: {cpu['usage']}%")

    # 内存
    mem = system.get_memory_info()
    used_gb = mem['used'] / 1024 / 1024 / 1024
    total_gb = mem['total'] / 1024 / 1024 / 1024
    print(f"内存: {used_gb:.1f}GB / {total_gb:.1f}GB ({mem['usage']:.1f}%)")

    # 磁盘
    disks = system.get_disk_info()
    for disk in disks:
        print(f"磁盘 {disk['drive']}: {disk['usage']:.1f}% 使用")

    print()

# 每 5 秒打印一次系统状态
while True:
    print_system_status()
    time.sleep(5)
```

== Lua

```lua
local system = require("wingman.system")

local function printSystemStatus()
    -- CPU
    local cpu = system.getCpuInfo()
    print("CPU: " .. cpu.brand)
    print("  使用率: " .. cpu.usage .. "%")

    -- 内存
    local mem = system.getMemoryInfo()
    local usedGb = mem.used / 1024 / 1024 / 1024
    local totalGb = mem.total / 1024 / 1024 / 1024
    print(string.format("内存: %.1fGB / %.1fGB (%.1f%%)", usedGb, totalGb, mem.usage))

    -- 磁盘
    local disks = system.getDiskInfo()
    for i, disk in ipairs(disks) do
        print(string.format("磁盘 %s: %.1f%% 使用", disk.drive, disk.usage))
    end

    print()
end

-- 每 5 秒打印一次系统状态
while true do
    printSystemStatus()
    os.execute("timeout /t 5 >nul")
end
```

:::

### 检查系统资源是否充足

:::tabs

== Python

```python
from wingman import system

def check_resources():
    """检查系统资源是否充足"""
    cpu = system.get_cpu_info()
    mem = system.get_memory_info()

    if cpu['usage'] > 90:
        print("警告: CPU 使用率过高")
        return False

    if mem['usage'] > 90:
        print("警告: 内存使用率过高")
        return False

    print("系统资源充足")
    return True

if check_resources():
    # 继续执行任务
    pass
```

== Lua

```lua
local system = require("wingman.system")

local function checkResources()
    local cpu = system.getCpuInfo()
    local mem = system.getMemoryInfo()

    if cpu.usage > 90 then
        print("警告: CPU 使用率过高")
        return false
    end

    if mem.usage > 90 then
        print("警告: 内存使用率过高")
        return false
    end

    print("系统资源充足")
    return true
end

if checkResources() then
    -- 继续执行任务
end
```

:::

---

## 可用接口

### `get_cpu_info()` / `getCpuInfo()`

获取 CPU 详细信息。

**返回：**
- `dict/table` - CPU 信息对象
  - `vendor` - 制造商（如 "GenuineIntel"）
  - `brand` - 型号（如 "Intel(R) Core(TM) i7-10700K"）
  - `cores` - 物理核心数
  - `threads` - 逻辑线程数
  - `maxClock` - 最大频率（MHz）
  - `currentClock` - 当前频率（MHz）
  - `usage` - 使用率（0-100）
  - `temperature` - 温度（摄氏度，如果可用）

### `get_cpu_usage()` / `getCpuUsage()`

快速获取 CPU 使用率。

**返回：**
- `number` - CPU 使用率（0-100）

### `get_memory_info()` / `getMemoryInfo()`

获取内存信息。

**返回：**
- `dict/table` - 内存信息对象
  - `total` - 总内存（字节）
  - `available` - 可用内存（字节）
  - `used` - 已用内存（字节）
  - `usage` - 使用率（0-100）

### `get_disk_info(drive?)` / `getDiskInfo(drive?)`

获取磁盘信息。

**参数：**
- `drive` - 可选，指定驱动器（如 "C:"），不指定则返回所有磁盘

**返回：**
- `dict/table` 或 `array` - 单个磁盘信息或磁盘数组
  - `drive` - 驱动器号
  - `total` - 总容量（字节）
  - `free` - 可用空间（字节）
  - `used` - 已用空间（字节）
  - `usage` - 使用率（0-100）
  - `fileSystem` - 文件系统类型（如 "NTFS"）

### `get_gpu_info()` / `getGpuInfo()`

获取 GPU 信息。

**返回：**
- `dict/table` - GPU 信息对象
  - `name` - 显卡名称

### `get_os_info()` / `getOsInfo()`

获取操作系统信息。

**返回：**
- `dict/table` - 系统信息对象
  - `platform` - 平台名称（如 "Windows"）
  - `version` - 版本号
  - `build` - 构建号
  - `architecture` - 架构（如 "AMD64"）
  - `computerName` - 计算机名
  - `userName` - 当前用户名
