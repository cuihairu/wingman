> [中文](security.md) | **English**

# API: wingman.security

Security module providing environment detection, encryption and decryption, random offsets, and other security-related functionality.

## Module Overview

The security module provides security features:
- **Environment detection** - Debugger detection, virtual machine detection, integrity verification
- **Humanized randomness** - Random delays, random coordinate offsets
- **Encryption and decryption** - String encryption and decryption
- **Hashing and randomness** - String hashing, random string generation
- **Sensitive information filtering** - Filter sensitive fields

---

## Detecting a Debugger

### is_debugger_present() / isDebuggerPresent()

**Description**: Detects whether it is running under a debugger.

**Signature**:

```python
is_debugger_present() -> bool
```

```lua
isDebuggerPresent() -> boolean
```

**Returns**:
- Whether a debugger was detected

---

## Detecting a Virtual Machine

### is_running_in_vm() / isRunningInVM()

**Description**: Detects whether it is running inside a virtual machine environment.

**Signature**:

```python
is_running_in_vm() -> bool
```

```lua
isRunningInVM() -> boolean
```

**Returns**:
- Whether it is running in a virtual machine

---

## Verifying Integrity

### verify_integrity() / verifyIntegrity()

**Description**: Verifies program integrity. **Currently a placeholder implementation**: by default (integrity checking not enabled) it always returns `True`/`true`; when explicitly enabled, it takes the unimplemented branch, always returns `False`/`false`, and logs a warning. Do not rely on it to decide whether the program has been tampered with.

**Signature**:

```python
verify_integrity() -> bool
```

```lua
verifyIntegrity() -> boolean
```

**Returns**:
- A placeholder value (see above), not a real integrity verification result

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

## Getting a Random Delay

### get_random_delay() / getRandomDelay()

**Description**: Gets a humanized random delay.

**Signature**:

```python
get_random_delay() -> int
```

```lua
getRandomDelay() -> number
```

**Returns**:
- A random delay (in milliseconds)

---

## Getting a Random Offset

### get_random_offset() / getRandomOffset()

**Description**: Gets a humanized random coordinate offset.

**Signature**:

```python
get_random_offset() -> list[float, float]
```

```lua
getRandomOffset() -> table
```

**Returns**: A **single array value** `[x_offset, y_offset]` (same shape on both ends, not multiple return values):
- Index `1` (Python `result[0]`) - X offset
- Index `2` (Python `result[1]`) - Y offset

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

> [The former `security.encryptString` / `security.decryptString` (XOR obfuscation, not real encryption)]
> were removed in 2026-09. For encryption, use the crypto module's `crypto.encryptAES` / `crypto.decryptAES`
> (AES-256-GCM); see [crypto API](crypto.md).

---

## Computing a Hash

### hash_string(str) / hashString(str)

**Description**: Computes the hash of a string.

**Signature**:

```python
hash_string(str: str) -> str
```

```lua
hashString(str: string) -> string
```

**Parameters**:
- `str` - The string to hash

**Returns**:
- The hash value (a hexadecimal string)

---

## Generating a Random String

### generate_random_string(length) / generateRandomString(length)

**Description**: Generates a random string.

**Signature**:

```python
generate_random_string(length: int) -> str
```

```lua
generateRandomString(length: number) -> string
```

**Parameters**:
- `length` - String length

**Returns**:
- The random string

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

## Filtering Sensitive Information

### filter_sensitive(str) / filterSensitive(str)

**Description**: Filters sensitive information from a string. The current implementation does **key-name masking**: it replaces matched sensitive key names themselves (`password`/`passwd`/`pwd`/`token`/`key`/`secret`/`api_key`/`apikey`, case-insensitive) with `***`; it does **not mask values** — whatever follows `=` is kept as-is.

**Signature**:

```python
filter_sensitive(str: str) -> str
```

```lua
filterSensitive(str: string) -> string
```

**Parameters**:
- `str` - The string to filter

**Returns**:
- The string with sensitive **key names** replaced by `***` (values are not replaced)

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

## Available APIs

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `is_debugger_present()` | `isDebuggerPresent()` | Detect a debugger | Returns: whether a debugger was detected |
| `is_running_in_vm()` | `isRunningInVM()` | Detect a virtual machine | Returns: whether running in a VM |
| `verify_integrity()` | `verifyIntegrity()` | Verify integrity | Returns: placeholder implementation (always true by default; always false when explicitly enabled) |
| `get_random_delay()` | `getRandomDelay()` | Get a random delay | Returns: milliseconds |
| `get_random_offset()` | `getRandomOffset()` | Get a random offset | Returns: [x, y] array (Lua uses offset[1]/offset[2]) |
| `hash_string(str)` | `hashString(str)` | Compute a hash | str: the string<br>Returns: hash value |
| `generate_random_string(length)` | `generateRandomString(length)` | Generate a random string | length: string length<br>Returns: the random string |
| `filter_sensitive(str)` | `filterSensitive(str)` | Filter sensitive information | str: original string<br>Returns: string with key names masked (values not replaced) |
