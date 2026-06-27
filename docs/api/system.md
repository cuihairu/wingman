# API: wingman.system

系统模块，提供 CPU、内存、磁盘、GPU 和操作系统信息查询功能。

## 模块概述

system 模块提供系统信息查询功能：
- **CPU 信息** - 处理器型号、核心数、使用率
- **内存信息** - 总内存、可用内存、使用率
- **磁盘信息** - 磁盘容量、可用空间、使用率
- **GPU 信息** - 显卡名称
- **系统信息** - 操作系统版本、架构、计算机名
- **网络适配器** - 适配器名称、MAC、IP、状态
- **显示器** - 分辨率、刷新率、主显示器
- **运行时间** - 系统开机以来的运行秒数
- **日期时间** - 当前日期时间、时区
- **进程/线程统计** - 系统进程数与线程数

---

## 获取 CPU 信息

### get_cpu_info() / getCpuInfo()

**说明**：获取 CPU 详细信息。

**函数签名**：

```python
get_cpu_info() -> dict
```

```lua
getCpuInfo() -> table
```

**返回**：
- CPU 信息对象：
  - `vendor` / `vendor` - 制造商（如 "GenuineIntel"）
  - `brand` / `brand` - 型号（如 "Intel(R) Core(TM) i7-10700K"）
  - `cores` / `cores` - 物理核心数
  - `threads` / `threads` - 逻辑线程数
  - `maxClock` / `maxClock` - 最大频率（MHz）
  - `currentClock` / `currentClock` - 当前频率（MHz）
  - `usage` / `usage` - 使用率（0-100）
  - `temperature` / `temperature` - 温度（摄氏度，如果可用）

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取 CPU 信息
cpu = system.get_cpu_info()
print(f"处理器: {cpu['brand']}")
print(f"核心数: {cpu['cores']}")
print(f"线程数: {cpu['threads']}")
print(f"使用率: {cpu['usage']}%")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取 CPU 信息
local cpu = wingman.system.getCpuInfo()
print("处理器: " .. cpu.brand)
print("核心数: " .. cpu.cores)
print("线程数: " .. cpu.threads)
print("使用率: " .. cpu.usage .. "%")
```

:::

---

## 获取 CPU 使用率

### get_cpu_usage() / getCpuUsage()

**说明**：快速获取 CPU 使用率。

**函数签名**：

```python
get_cpu_usage() -> float
```

```lua
getCpuUsage() -> number
```

**返回**：
- CPU 使用率（0-100）

---

## 获取内存信息

### get_memory_info() / getMemoryInfo()

**说明**：获取内存信息。

**函数签名**：

```python
get_memory_info() -> dict
```

```lua
getMemoryInfo() -> table
```

**返回**：
- 内存信息对象：
  - `total` / `total` - 总内存（字节）
  - `available` / `available` - 可用内存（字节）
  - `used` / `used` - 已用内存（字节）
  - `usage` / `usage` - 使用率（0-100）

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取内存信息
mem = system.get_memory_info()
print(f"总内存: {mem['total'] / 1024 / 1024 / 1024:.2f} GB")
print(f"可用内存: {mem['available'] / 1024 / 1024 / 1024:.2f} GB")
print(f"使用率: {mem['usage']:.1f}%")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取内存信息
local mem = wingman.system.getMemoryInfo()
print("总内存: " .. string.format("%.2f", mem.total / 1024 / 1024 / 1024) .. " GB")
print("可用内存: " .. string.format("%.2f", mem.available / 1024 / 1024 / 1024) .. " GB")
print("使用率: " .. string.format("%.1f", mem.usage) .. "%")
```

:::

---

## 获取磁盘信息

### get_disk_info(drive?) / getDiskInfo(drive?)

**说明**：获取磁盘信息。

**函数签名**：

```python
get_disk_info(drive: str = None) -> dict | list[dict]
```

```lua
getDiskInfo(drive: string = nil) -> table | table
```

**参数**：
- `drive` - 可选，指定驱动器（如 "C:"），不指定则返回所有磁盘

**返回**：
- Python: 单个磁盘信息字典或磁盘信息列表
- Lua: 单个磁盘信息表格或磁盘信息数组

**磁盘信息对象包含**：
- `drive` / `drive` - 驱动器号
- `total` / `total` - 总容量（字节）
- `free` / `free` - 可用空间（字节）
- `used` / `used` - 已用空间（字节）
- `usage` / `usage` - 使用率（0-100）
- `fileSystem` / `fileSystem` - 文件系统类型（如 "NTFS"）

:::tabs

== Python

```python:line-numbers
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

```lua:line-numbers
local wingman = require("wingman")

-- 获取所有磁盘信息
local disks = wingman.system.getDiskInfo()
for i, disk in ipairs(disks) do
    print(disk.drive .. ": " .. string.format("%.1f", disk.used / 1024 / 1024 / 1024) ..
          "GB / " .. string.format("%.1f", disk.total / 1024 / 1024 / 1024) .. "GB")
end

-- 获取指定磁盘信息
local cDrive = wingman.system.getDiskInfo("C:")
print("C 盘使用率: " .. string.format("%.1f", cDrive.usage) .. "%")
```

:::

---

## 获取 GPU 信息

### get_gpu_info() / getGpuInfo()

**说明**：获取 GPU 信息。

**函数签名**：

```python
get_gpu_info() -> dict
```

```lua
getGpuInfo() -> table
```

**返回**：
- GPU 信息对象：
  - `name` / `name` - 显卡名称

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取 GPU 信息
gpu = system.get_gpu_info()
print(f"显卡: {gpu['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取 GPU 信息
local gpu = wingman.system.getGpuInfo()
print("显卡: " .. gpu.name)
```

:::

---

## 获取系统信息

### get_os_info() / getOsInfo()

**说明**：获取操作系统信息。

**函数签名**：

```python
get_os_info() -> dict
```

```lua
getOsInfo() -> table
```

**返回**：
- 系统信息对象：
  - `platform` / `platform` - 平台名称（如 "Windows"）
  - `version` / `version` - 版本号
  - `build` / `build` - 构建号
  - `architecture` / `architecture` - 架构（如 "AMD64"）
  - `computerName` / `computerName` - 计算机名
  - `userName` / `userName` - 当前用户名

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取系统信息
os_info = system.get_os_info()
print(f"系统: {os_info['platform']} {os_info['version']}")
print(f"架构: {os_info['architecture']}")
print(f"计算机名: {os_info['computerName']}")
print(f"用户名: {os_info['userName']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取系统信息
local osInfo = wingman.system.getOsInfo()
print("系统: " .. osInfo.platform .. " " .. osInfo.version)
print("架构: " .. osInfo.architecture)
print("计算机名: " .. osInfo.computerName)
print("用户名: " .. osInfo.userName)
```

:::

---

## 获取网络适配器信息

### get_network_adapters() / getNetworkAdapters()

**说明**：获取系统网络适配器信息。

**函数签名**：

```python
get_network_adapters() -> list[dict]
```

```lua
getNetworkAdapters() -> table
```

**返回**：
- 网络适配器信息数组，每个适配器对象包含：
  - `name` / `name` - 适配器名称
  - `description` / `description` - 适配器描述
  - `macAddress` / `macAddress` - MAC 地址
  - `ipAddress` / `ipAddress` - IP 地址
  - `isUp` / `isUp` - 是否启用

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取网络适配器信息
adapters = system.get_network_adapters()
for adapter in adapters:
    print(f"名称: {adapter['name']}")
    print(f"IP: {adapter['ipAddress']}")
    print(f"启用: {adapter['isUp']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取网络适配器信息
local adapters = wingman.system.getNetworkAdapters()
for i, adapter in ipairs(adapters) do
    print("名称: " .. adapter.name)
    print("IP: " .. adapter.ipAddress)
    print("启用: " .. tostring(adapter.isUp))
end
```

:::

---

## 获取显示器信息

### get_display_info() / getDisplayInfo()

**说明**：获取系统显示器信息。

**函数签名**：

```python
get_display_info() -> list[dict]
```

```lua
getDisplayInfo() -> table
```

**返回**：
- 显示器信息数组，每个显示器对象包含：
  - `index` / `index` - 显示器索引
  - `name` / `name` - 显示器名称
  - `width` / `width` - 分辨率宽度（像素）
  - `height` / `height` - 分辨率高度（像素）
  - `refreshRate` / `refreshRate` - 刷新率（Hz）
  - `isPrimary` / `isPrimary` - 是否为主显示器

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取显示器信息
displays = system.get_display_info()
for display in displays:
    print(f"显示器 {display['index']}: {display['width']}x{display['height']}")
    print(f"主显示器: {display['isPrimary']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取显示器信息
local displays = wingman.system.getDisplayInfo()
for i, display in ipairs(displays) do
    print("显示器 " .. display.index .. ": " .. display.width .. "x" .. display.height)
    print("主显示器: " .. tostring(display.isPrimary))
end
```

:::

---

## 获取系统运行时间

### get_uptime() / getUptime()

**说明**：获取系统开机以来的运行时间（秒）。

**函数签名**：

```python
get_uptime() -> int
```

```lua
getUptime() -> number
```

**返回**：
- 系统运行时间（秒）

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取运行时间
uptime = system.get_uptime()
print(f"运行时间: {uptime // 3600} 小时 {(uptime % 3600) // 60} 分钟")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取运行时间
local uptime = wingman.system.getUptime()
local hours = math.floor(uptime / 3600)
local minutes = math.floor((uptime % 3600) / 60)
print("运行时间: " .. hours .. " 小时 " .. minutes .. " 分钟")
```

:::

---

## 获取当前日期时间

### get_date_time() / getDateTime()

**说明**：获取系统当前日期时间的字符串表示。

**函数签名**：

```python
get_date_time() -> str
```

```lua
getDateTime() -> string
```

**返回**：
- 当前日期时间字符串

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取当前日期时间
dt = system.get_date_time()
print(f"当前时间: {dt}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取当前日期时间
local dt = wingman.system.getDateTime()
print("当前时间: " .. dt)
```

:::

---

## 获取时区

### get_time_zone() / getTimeZone()

**说明**：获取系统时区字符串。

**函数签名**：

```python
get_time_zone() -> str
```

```lua
getTimeZone() -> string
```

**返回**：
- 系统时区字符串

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取时区
tz = system.get_time_zone()
print(f"时区: {tz}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取时区
local tz = wingman.system.getTimeZone()
print("时区: " .. tz)
```

:::

---

## 获取进程数

### get_process_count() / getProcessCount()

**说明**：获取系统当前运行的进程总数。

**函数签名**：

```python
get_process_count() -> int
```

```lua
getProcessCount() -> number
```

**返回**：
- 进程总数

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取进程数
count = system.get_process_count()
print(f"进程数: {count}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取进程数
local count = wingman.system.getProcessCount()
print("进程数: " .. count)
```

:::

---

## 获取线程数

### get_thread_count() / getThreadCount()

**说明**：获取系统当前运行的线程总数。

**函数签名**：

```python
get_thread_count() -> int
```

```lua
getThreadCount() -> number
```

**返回**：
- 线程总数

:::tabs

== Python

```python:line-numbers
from wingman import system

# 获取线程数
count = system.get_thread_count()
print(f"线程数: {count}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取线程数
local count = wingman.system.getThreadCount()
print("线程数: " .. count)
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `get_cpu_info()` | `getCpuInfo()` | 获取CPU信息 | 返回: CPU信息对象 |
| `get_cpu_usage()` | `getCpuUsage()` | 获取CPU使用率 | 返回: 使用率(0-100) |
| `get_memory_info()` | `getMemoryInfo()` | 获取内存信息 | 返回: 内存信息对象 |
| `get_disk_info(drive?)` | `getDiskInfo(drive?)` | 获取磁盘信息 | drive: 驱动器号(可选)<br>返回: 磁盘信息对象或数组 |
| `get_gpu_info()` | `getGpuInfo()` | 获取GPU信息 | 返回: GPU信息对象 |
| `get_os_info()` | `getOsInfo()` | 获取系统信息 | 返回: 系统信息对象 |
| `get_network_adapters()` | `getNetworkAdapters()` | 获取网络适配器信息 | 返回: 网络适配器信息数组 |
| `get_display_info()` | `getDisplayInfo()` | 获取显示器信息 | 返回: 显示器信息数组 |
| `get_uptime()` | `getUptime()` | 获取系统运行时间 | 返回: 运行秒数 |
| `get_date_time()` | `getDateTime()` | 获取当前日期时间 | 返回: 日期时间字符串 |
| `get_time_zone()` | `getTimeZone()` | 获取系统时区 | 返回: 时区字符串 |
| `get_process_count()` | `getProcessCount()` | 获取进程总数 | 返回: 进程数 |
| `get_thread_count()` | `getThreadCount()` | 获取线程总数 | 返回: 线程数 |

---

## 返回值结构

### CPU 信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `vendor` | string | 制造商（如 "GenuineIntel"） |
| `brand` | string | 型号（如 "Intel(R) Core(TM) i7-10700K"） |
| `cores` | number | 物理核心数 |
| `threads` | number | 逻辑线程数 |
| `maxClock` | number | 最大频率（MHz） |
| `currentClock` | number | 当前频率（MHz） |
| `usage` | number | 使用率（0-100） |
| `temperature` | number | 温度（摄氏度，如果可用） |

### 内存信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `total` | number | 总内存（字节） |
| `available` | number | 可用内存（字节） |
| `used` | number | 已用内存（字节） |
| `usage` | number | 使用率（0-100） |

### 磁盘信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `drive` | string | 驱动器号 |
| `total` | number | 总容量（字节） |
| `free` | number | 可用空间（字节） |
| `used` | number | 已用空间（字节） |
| `usage` | number | 使用率（0-100） |
| `fileSystem` | string | 文件系统类型（如 "NTFS"） |

### GPU 信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 显卡名称 |

### 系统信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `platform` | string | 平台名称（如 "Windows"） |
| `version` | string | 版本号 |
| `build` | string | 构建号 |
| `architecture` | string | 架构（如 "AMD64"） |
| `computerName` | string | 计算机名 |
| `userName` | string | 当前用户名 |

### 网络适配器信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 适配器名称 |
| `description` | string | 适配器描述 |
| `macAddress` | string | MAC 地址 |
| `ipAddress` | string | IP 地址 |
| `isUp` | boolean | 是否启用 |

### 显示器信息对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `index` | number | 显示器索引 |
| `name` | string | 显示器名称 |
| `width` | number | 分辨率宽度（像素） |
| `height` | number | 分辨率高度（像素） |
| `refreshRate` | number | 刷新率（Hz） |
| `isPrimary` | boolean | 是否为主显示器 |
