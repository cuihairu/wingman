# API: wingman.security

安全模块，提供环境检测、加密解密、随机偏移等安全相关功能。

## 模块概述

security 模块提供安全功能：
- **环境检测** - 调试器检测、虚拟机检测、完整性验证
- **人性化随机** - 随机延迟、随机坐标偏移
- **加密解密** - 字符串加密解密
- **哈希和随机** - 字符串哈希、随机字符串生成
- **敏感信息过滤** - 过滤敏感字段

---

## 检测调试器

### is_debugger_present() / isDebuggerPresent()

**说明**：检测是否在调试器中运行。

**函数签名**：

```python
is_debugger_present() -> bool
```

```lua
isDebuggerPresent() -> boolean
```

**返回**：
- 是否检测到调试器

---

## 检测虚拟机

### is_running_in_vm() / isRunningInVM()

**说明**：检测是否在虚拟机环境中运行。

**函数签名**：

```python
is_running_in_vm() -> bool
```

```lua
isRunningInVM() -> boolean
```

**返回**：
- 是否在虚拟机中

---

## 验证完整性

### verify_integrity() / verifyIntegrity()

**说明**：验证程序完整性。**当前为占位实现**：默认（完整性检查开关未开启）恒返回 `True`/`true`；显式开启后走未实现分支，恒返回 `False`/`false` 并打警告日志。不能据此判断程序是否被篡改。

**函数签名**：

```python
verify_integrity() -> bool
```

```lua
verifyIntegrity() -> boolean
```

**返回**：
- 占位值（见上），非真实完整性校验结果

:::tabs

== Python

```python:line-numbers
from wingman import security

# 检测是否在调试器中运行
if security.is_debugger_present():
    print("警告: 检测到调试器")

# 检测是否在虚拟机中运行
if security.is_running_in_vm():
    print("警告: 检测到虚拟机环境")

# 验证程序完整性
if not security.verify_integrity():
    print("警告: 程序完整性验证失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 检测是否在调试器中运行
if wingman.security.isDebuggerPresent() then
    print("警告: 检测到调试器")
end

-- 检测是否在虚拟机中运行
if wingman.security.isRunningInVM() then
    print("警告: 检测到虚拟机环境")
end

-- 验证程序完整性
if not wingman.security.verifyIntegrity() then
    print("警告: 程序完整性验证失败")
end
```

:::

---

## 获取随机延迟

### get_random_delay() / getRandomDelay()

**说明**：获取人性化随机延迟时间。

**函数签名**：

```python
get_random_delay() -> int
```

```lua
getRandomDelay() -> number
```

**返回**：
- 随机延迟（毫秒）

---

## 获取随机偏移

### get_random_offset() / getRandomOffset()

**说明**：获取人性化随机坐标偏移。

**函数签名**：

```python
get_random_offset() -> list[float, float]
```

```lua
getRandomOffset() -> table
```

**返回**：**单个数组值** `[x_offset, y_offset]`（两端同构，不是多返回值）：
- 下标 `1`（Python `result[0]`）- X 偏移
- 下标 `2`（Python `result[1]`）- Y 偏移

:::tabs

== Python

```python:line-numbers
from wingman import security, input

# 获取随机延迟（毫秒）
delay = security.get_random_delay()

# 获取随机坐标偏移（返回列表，可直接解包）
offset_x, offset_y = security.get_random_offset()
# 在目标坐标基础上添加随机偏移
input.click(100 + offset_x, 200 + offset_y)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取随机延迟（毫秒）
local delay = wingman.security.getRandomDelay()

-- 获取随机坐标偏移（Lua 返回一张数组表，不能多重赋值解构）
local offset = wingman.security.getRandomOffset()
-- 在目标坐标基础上添加随机偏移
wingman.input.click(100 + offset[1], 200 + offset[2])
```

:::

> [原 `security.encryptString` / `security.decryptString`（XOR 混淆，非真实加密）]
> 已于 2026-09 移除。加密请使用 crypto 模块的 `crypto.encryptAES` / `crypto.decryptAES`
> （AES-256-GCM），见 [crypto API](crypto.md)。

---

## 计算哈希

### hash_string(str) / hashString(str)

**说明**：计算字符串的哈希值。

**函数签名**：

```python
hash_string(str: str) -> str
```

```lua
hashString(str: string) -> string
```

**参数**：
- `str` - 要哈希的字符串

**返回**：
- 哈希值（十六进制字符串）

---

## 生成随机字符串

### generate_random_string(length) / generateRandomString(length)

**说明**：生成随机字符串。

**函数签名**：

```python
generate_random_string(length: int) -> str
```

```lua
generateRandomString(length: number) -> string
```

**参数**：
- `length` - 字符串长度

**返回**：
- 随机字符串

:::tabs

== Python

```python:line-numbers
from wingman import security

# 计算字符串哈希
hash_value = security.hash_string("my_data")
print(f"哈希值: {hash_value}")

# 生成随机字符串
random_str = security.generate_random_string(16)
print(f"随机字符串: {random_str}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 计算字符串哈希
local hashValue = wingman.security.hashString("my_data")
print("哈希值: " .. hashValue)

-- 生成随机字符串
local randomStr = wingman.security.generateRandomString(16)
print("随机字符串: " .. randomStr)
```

:::

---

## 过滤敏感信息

### filter_sensitive(str) / filterSensitive(str)

**说明**：过滤字符串中的敏感信息。当前实现是**键名打码**：把命中的敏感键名本身（`password`/`passwd`/`pwd`/`token`/`key`/`secret`/`api_key`/`apikey`，大小写不敏感）替换为 `***`，**不是值打码**——`=` 后面的值原样保留。

**函数签名**：

```python
filter_sensitive(str: str) -> str
```

```lua
filterSensitive(str: string) -> string
```

**参数**：
- `str` - 要过滤的字符串

**返回**：
- 敏感**键名**被替换为 `***` 的字符串（值不替换）

:::tabs

== Python

```python:line-numbers
from wingman import security

# 键名打码（非值打码）
safe_string = security.filter_sensitive("password=123456&token=abc")
print(safe_string)  # "***=123456&***=abc"
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 键名打码（非值打码）
local safeString = wingman.security.filterSensitive("password=123456&token=abc")
print(safeString)  -- "***=123456&***=abc"
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `is_debugger_present()` | `isDebuggerPresent()` | 检测调试器 | 返回: 是否检测到调试器 |
| `is_running_in_vm()` | `isRunningInVM()` | 检测虚拟机 | 返回: 是否在虚拟机中 |
| `verify_integrity()` | `verifyIntegrity()` | 验证完整性 | 返回: 占位实现（默认恒 true；显式开启后恒 false） |
| `get_random_delay()` | `getRandomDelay()` | 获取随机延迟 | 返回: 毫秒数 |
| `get_random_offset()` | `getRandomOffset()` | 获取随机偏移 | 返回: [x, y] 数组（Lua 取 offset[1]/offset[2]） |
| `hash_string(str)` | `hashString(str)` | 计算哈希 | str: 字符串<br>返回: 哈希值 |
| `generate_random_string(length)` | `generateRandomString(length)` | 生成随机字符串 | length: 字符串长度<br>返回: 随机字符串 |
| `filter_sensitive(str)` | `filterSensitive(str)` | 过滤敏感信息 | str: 原字符串<br>返回: 键名打码后字符串（值不替换） |
