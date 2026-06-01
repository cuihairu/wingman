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

**说明**：验证程序完整性。

**函数签名**：

```python
verify_integrity() -> bool
```

```lua
verifyIntegrity() -> boolean
```

**返回**：
- 程序是否未被篡改

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
local security = require("wingman.security")

-- 检测是否在调试器中运行
if security.isDebuggerPresent() then
    print("警告: 检测到调试器")
end

-- 检测是否在虚拟机中运行
if security.isRunningInVM() then
    print("警告: 检测到虚拟机环境")
end

-- 验证程序完整性
if not security.verifyIntegrity() then
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
get_random_offset() -> tuple[int, int]
```

```lua
getRandomOffset() -> number, number
```

**返回**：
- Python: `(x_offset, y_offset)` 元组
- Lua: 两个返回值 `x_offset, y_offset`

:::tabs

== Python

```python:line-numbers
from wingman import security, input

# 获取随机延迟（毫秒）
delay = security.get_random_delay()

# 获取随机坐标偏移
offset_x, offset_y = security.get_random_offset()
# 在目标坐标基础上添加随机偏移
input.click(100 + offset_x, 200 + offset_y)
```

== Lua

```lua:line-numbers
local security = require("wingman.security")
local input = require("wingman.input")

-- 获取随机延迟（毫秒）
local delay = security.getRandomDelay()

-- 获取随机坐标偏移
local offsetX, offsetY = security.getRandomOffset()
-- 在目标坐标基础上添加随机偏移
input.click(100 + offsetX, 200 + offsetY)
```

:::

---

## 加密字符串

### encrypt_string(str, key) / encryptString(str, key)

**说明**：加密字符串。

**函数签名**：

```python
encrypt_string(str: str, key: str) -> str
```

```lua
encryptString(str: string, key: string) -> string
```

**参数**：
- `str` - 要加密的字符串
- `key` - 加密密钥

**返回**：
- 加密后的字符串

---

## 解密字符串

### decrypt_string(str, key) / decryptString(str, key)

**说明**：解密字符串。

**函数签名**：

```python
decrypt_string(str: str, key: str) -> str
```

```lua
decryptString(str: string, key: string) -> string
```

**参数**：
- `str` - 要解密的字符串
- `key` - 解密密钥

**返回**：
- 解密后的字符串

:::tabs

== Python

```python:line-numbers
from wingman import security

# 加密字符串
key = "my_secret_key"
encrypted = security.encrypt_string("sensitive_data", key)
print(f"加密后: {encrypted}")

# 解密字符串
decrypted = security.decrypt_string(encrypted, key)
print(f"解密后: {decrypted}")
```

== Lua

```lua:line-numbers
local security = require("wingman.security")

-- 加密字符串
local key = "my_secret_key"
local encrypted = security.encryptString("sensitive_data", key)
print("加密后: " .. encrypted)

-- 解密字符串
local decrypted = security.decryptString(encrypted, key)
print("解密后: " .. decrypted)
```

:::

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
local security = require("wingman.security")

-- 计算字符串哈希
local hashValue = security.hashString("my_data")
print("哈希值: " .. hashValue)

-- 生成随机字符串
local randomStr = security.generateRandomString(16)
print("随机字符串: " .. randomStr)
```

:::

---

## 过滤敏感信息

### filter_sensitive(str) / filterSensitive(str)

**说明**：过滤字符串中的敏感信息。

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
- 敏感信息被替换为 `***` 的字符串

:::tabs

== Python

```python:line-numbers
from wingman import security

# 过滤敏感信息（如密码、token等）
safe_string = security.filter_sensitive("password=123456&token=abc")
print(safe_string)  # "password=***&token=***"
```

== Lua

```lua:line-numbers
local security = require("wingman.security")

-- 过滤敏感信息（如密码、token等）
local safeString = security.filterSensitive("password=123456&token=abc")
print(safeString)  -- "password=***&token=***"
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `is_debugger_present()` | `isDebuggerPresent()` | 检测调试器 | 返回: 是否检测到调试器 |
| `is_running_in_vm()` | `isRunningInVM()` | 检测虚拟机 | 返回: 是否在虚拟机中 |
| `verify_integrity()` | `verifyIntegrity()` | 验证完整性 | 返回: 是否未被篡改 |
| `get_random_delay()` | `getRandomDelay()` | 获取随机延迟 | 返回: 毫秒数 |
| `get_random_offset()` | `getRandomOffset()` | 获取随机偏移 | 返回: X和Y偏移量 |
| `hash_string(str)` | `hashString(str)` | 计算哈希 | str: 字符串<br>返回: 哈希值 |
| `encrypt_string(str, key)` | `encryptString(str, key)` | 加密字符串 | str: 明文<br>key: 密钥<br>返回: 密文 |
| `decrypt_string(str, key)` | `decryptString(str, key)` | 解密字符串 | str: 密文<br>key: 密钥<br>返回: 明文 |
| `generate_random_string(length)` | `generateRandomString(length)` | 生成随机字符串 | length: 字符串长度<br>返回: 随机字符串 |
| `filter_sensitive(str)` | `filterSensitive(str)` | 过滤敏感信息 | str: 原字符串<br>返回: 过滤后字符串 |
