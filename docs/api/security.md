# Security API

`wingman.security` 提供安全相关功能，包括反调试检测、虚拟机检测、字符串加密和随机数生成。

## 环境检测

:::tabs

== Python

```python
from wingman import security

# 检测是否在调试器中运行
if security.is_debugger_present():
    print("警告: 检测到调试器")
    # 采取保护措施

# 检测是否在虚拟机中运行
if security.is_running_in_vm():
    print("警告: 检测到虚拟机环境")
    # 限制功能或退出

# 验证程序完整性
if not security.verify_integrity():
    print("警告: 程序完整性验证失败")
    # 可能被篡改
```

== Lua

```lua
local security = require("wingman.security")

-- 检测是否在调试器中运行
if security.isDebuggerPresent() then
    print("警告: 检测到调试器")
    -- 采取保护措施
end

-- 检测是否在虚拟机中运行
if security.isRunningInVM() then
    print("警告: 检测到虚拟机环境")
    -- 限制功能或退出
end

-- 验证程序完整性
if not security.verifyIntegrity() then
    print("警告: 程序完整性验证失败")
    -- 可能被篡改
end
```

:::

## 人性化随机偏移

:::tabs

== Python

```python
from wingman import security, input

# 获取随机延迟（毫秒）
delay = security.get_random_delay()
import time
time.sleep(delay / 1000)

# 获取随机坐标偏移
offset_x, offset_y = security.get_random_offset()
# 在目标坐标基础上添加随机偏移
input.click(100 + offset_x, 200 + offset_y)
```

== Lua

```lua
local security = require("wingman.security")
local input = require("wingman.input")

-- 获取随机延迟（毫秒）
local delay = security.getRandomDelay()
-- 使用 delay 进行延迟

-- 获取随机坐标偏移
local offsetX, offsetY = security.getRandomOffset()
-- 在目标坐标基础上添加随机偏移
input.click(100 + offsetX, 200 + offsetY)
```

:::

## 字符串加密解密

:::tabs

== Python

```python
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

```lua
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

## 哈希和随机字符串

:::tabs

== Python

```python
from wingman import security

# 计算字符串哈希
hash_value = security.hash_string("my_data")
print(f"哈希值: {hash_value}")

# 生成随机字符串
random_str = security.generate_random_string(16)
print(f"随机字符串: {random_str}")
```

== Lua

```lua
local security = require("wingman.security")

-- 计算字符串哈希
local hashValue = security.hashString("my_data")
print("哈希值: " .. hashValue)

-- 生成随机字符串
local randomStr = security.generateRandomString(16)
print("随机字符串: " .. randomStr)
```

:::

## 敏感信息过滤

:::tabs

== Python

```python
from wingman import security

# 过滤敏感信息（如密码、token等）
safe_string = security.filter_sensitive("password=123456&token=abc")
print(safe_string)  # "password=***&token=***"
```

== Lua

```lua
local security = require("wingman.security")

-- 过滤敏感信息（如密码、token等）
local safeString = security.filterSensitive("password=123456&token=abc")
print(safeString)  -- "password=***&token=***"
```

:::

---

## 完整示例

### 安全保护脚本

:::tabs

== Python

```python
from wingman import security

def check_environment():
    """检查运行环境"""
    issues = []

    if security.is_debugger_present():
        issues.append("调试器检测")

    if security.is_running_in_vm():
        issues.append("虚拟机环境")

    if not security.verify_integrity():
        issues.append("程序完整性验证失败")

    return issues

def main():
    issues = check_environment()
    if issues:
        print(f"安全警告: {', '.join(issues)}")
        # 根据需要决定是否继续运行
        return False

    print("环境安全，继续运行")
    # 主逻辑
    return True

if __name__ == "__main__":
    main()
```

== Lua

```lua
local security = require("wingman.security")

local function checkEnvironment()
    local issues = {}

    if security.isDebuggerPresent() then
        table.insert(issues, "调试器检测")
    end

    if security.isRunningInVM() then
        table.insert(issues, "虚拟机环境")
    end

    if not security.verifyIntegrity() then
        table.insert(issues, "程序完整性验证失败")
    end

    return issues
end

local function main()
    local issues = checkEnvironment()
    if #issues > 0 then
        print("安全警告: " .. table.concat(issues, ", "))
        -- 根据需要决定是否继续运行
        return false
    end

    print("环境安全，继续运行")
    -- 主逻辑
    return true
end

main()
```

:::

### 人性化操作

:::tabs

== Python

```python
from wingman import security, input
import time

def human_click(x, y):
    """模拟人类点击，添加随机延迟和偏移"""
    # 随机延迟
    delay = security.get_random_delay()
    time.sleep(delay / 1000)

    # 随机偏移
    offset_x, offset_y = security.get_random_offset()
    input.click(x + offset_x, y + offset_y)

# 使用
human_click(100, 200)
human_click(300, 400)
```

== Lua

```lua
local security = require("wingman.security")
local input = require("wingman.input")

local function humanClick(x, y)
    -- 随机延迟
    local delay = security.getRandomDelay()
    -- 延迟 implementation specific

    -- 随机偏移
    local offsetX, offsetY = security.getRandomOffset()
    input.click(x + offsetX, y + offsetY)
end

-- 使用
humanClick(100, 200)
humanClick(300, 400)
```

:::

### 配置数据加密存储

:::tabs

== Python

```python
from wingman import security, kv

# 加密密钥（实际应用中应该从安全的地方获取）
ENCRYPTION_KEY = "your_secure_key"

def save_config(key, value):
    """加密保存配置"""
    encrypted = security.encrypt_string(str(value), ENCRYPTION_KEY)
    kv.set(key, encrypted)

def load_config(key, default=None):
    """解密加载配置"""
    encrypted = kv.get(key)
    if encrypted:
        decrypted = security.decrypt_string(encrypted, ENCRYPTION_KEY)
        return decrypted
    return default

# 使用
save_config("account", "user:password")
account = load_config("account")
```

== Lua

```lua
local security = require("wingman.security")
local kv = require("wingman.kv")

-- 加密密钥
local ENCRYPTION_KEY = "your_secure_key"

local function saveConfig(key, value)
    local encrypted = security.encryptString(tostring(value), ENCRYPTION_KEY)
    kv.set(key, encrypted)
end

local function loadConfig(key, default)
    local encrypted = kv.get(key)
    if encrypted then
        local decrypted = security.decryptString(encrypted, ENCRYPTION_KEY)
        return decrypted
    end
    return default
end

-- 使用
saveConfig("account", "user:password")
local account = loadConfig("account")
```

:::

---

## 可用接口

### `is_debugger_present()` / `isDebuggerPresent()`

检测是否在调试器中运行。

**返回：**
- `boolean` - 是否检测到调试器

### `is_running_in_vm()` / `isRunningInVM()`

检测是否在虚拟机环境中运行。

**返回：**
- `boolean` - 是否在虚拟机中

### `verify_integrity()` / `verifyIntegrity()`

验证程序完整性。

**返回：**
- `boolean` - 程序是否未被篡改

### `get_random_delay()` / `getRandomDelay()`

获取人性化随机延迟时间。

**返回：**
- `number` - 随机延迟（毫秒）

### `get_random_offset()` / `getRandomOffset()`

获取人性化随机坐标偏移。

**返回：**
- `number, number` - X 和 Y 方向的偏移量

### `hash_string(str)` / `hashString(str)`

计算字符串的哈希值。

**参数：**
- `str` - 要哈希的字符串

**返回：**
- `string` - 哈希值（十六进制字符串）

### `encrypt_string(str, key)` / `encryptString(str, key)`

加密字符串。

**参数：**
- `str` - 要加密的字符串
- `key` - 加密密钥

**返回：**
- `string` - 加密后的字符串

### `decrypt_string(str, key)` / `decryptString(str, key)`

解密字符串。

**参数：**
- `str` - 要解密的字符串
- `key` - 解密密钥

**返回：**
- `string` - 解密后的字符串

### `generate_random_string(length)` / `generateRandomString(length)`

生成随机字符串。

**参数：**
- `length` - 字符串长度

**返回：**
- `string` - 随机字符串

### `filter_sensitive(str)` / `filterSensitive(str)`

过滤字符串中的敏感信息。

**参数：**
- `str` - 要过滤的字符串

**返回：**
- `string` - 敏感信息被替换为 `***` 的字符串
