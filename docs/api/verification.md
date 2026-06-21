# API: wingman.verification

TOTP（双因素认证）纯工具函数模块。

## 模块概述

verification 模块提供无状态的 TOTP 工具函数：
- **TOTP 生成** - 生成基于时间的一次性验证码
- **Steam Guard** - 生成 Steam Guard 验证码
- **验证码校验** - 验证 TOTP 验证码是否有效
- **剩余时间** - 获取当前验证码剩余有效秒数

> 所有函数均为无状态纯函数，不涉及密钥持久化。如需存储密钥，请使用 `wingman.kv` 模块。

---

## 生成 TOTP 验证码

### totp(secret, digits?, period?) / totp(secret, digits?, period?)

**说明**：生成基于时间的一次性验证码（TOTP, RFC 6238）。

**函数签名**：

```python
totp(secret: str, digits: int = 6, period: int = 30) -> str
```

```lua
totp(secret: string, digits: number?, period: number?) -> string
```

**参数**：
- `secret` - Base32 编码的密钥
- `digits` - 验证码位数，默认 6
- `period` - 时间步长（秒），默认 30

**返回**：
- 数字验证码字符串

:::tabs

== Python

```python:line-numbers
from wingman import verification

# 默认 6 位，30 秒步长
code = verification.totp("JBSWY3DPEHPK3PXP")
print(f"TOTP: {code}")

# 自定义 8 位，60 秒步长
code = verification.totp("JBSWY3DPEHPK3PXP", digits=8, period=60)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 默认 6 位，30 秒步长
local code = wingman.verification.totp("JBSWY3DPEHPK3PXP")
print("TOTP: " .. code)

-- 自定义 8 位，60 秒步长
local code = wingman.verification.totp("JBSWY3DPEHPK3PXP", 8, 60)
```

:::

---

## 生成 Steam Guard 验证码

### steamGuard(secret) / steamGuard(secret)

**说明**：生成 Steam Guard 验证码（5 位字母）。

**函数签名**：

```python
steam_guard(secret: str) -> str
```

```lua
steamGuard(secret: string) -> string
```

**参数**：
- `secret` - Steam Guard 密钥

**返回**：
- 5 位字母验证码

:::tabs

== Python

```python:line-numbers
from wingman import verification

code = verification.steam_guard("STEAM_GUARD_SECRET")
print(f"Steam Guard: {code}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local code = wingman.verification.steamGuard("STEAM_GUARD_SECRET")
print("Steam Guard: " .. code)
```

:::

---

## 验证 TOTP 验证码

### verify(secret, code, digits?, period?, window?) / verify(secret, code, digits?, period?, window?)

**说明**：验证 TOTP 验证码是否有效。支持时间窗口容错。

**函数签名**：

```python
verify(secret: str, code: str, digits: int = 6, period: int = 30, window: int = 1) -> bool
```

```lua
verify(secret: string, code: string, digits: number?, period: number?, window: number?) -> boolean
```

**参数**：
- `secret` - Base32 编码的密钥
- `code` - 要验证的验证码
- `digits` - 验证码位数，默认 6
- `period` - 时间步长（秒），默认 30
- `window` - 时间窗口容错数，默认 1（允许前/后各 1 个周期）

**返回**：
- 验证码是否有效

:::tabs

== Python

```python:line-numbers
from wingman import verification

secret = "JBSWY3DPEHPK3PXP"
code = verification.totp(secret)

# 验证当前验证码
if verification.verify(secret, code):
    print("验证通过")

# 带容错窗口（允许前后各 2 个周期）
if verification.verify(secret, code, window=2):
    print("验证通过（宽松模式）")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local secret = "JBSWY3DPEHPK3PXP"
local code = wingman.verification.totp(secret)

-- 验证当前验证码
if wingman.verification.verify(secret, code) then
    print("验证通过")
end

-- 带容错窗口（允许前后各 2 个周期）
if wingman.verification.verify(secret, code, 6, 30, 2) then
    print("验证通过（宽松模式）")
end
```

:::

---

## 获取剩余有效时间

### remaining(period?) / remaining(period?)

**说明**：获取当前 TOTP 验证码剩余有效秒数。

**函数签名**：

```python
remaining(period: int = 30) -> int
```

```lua
remaining(period: number?) -> number
```

**参数**：
- `period` - 时间步长（秒），默认 30

**返回**：
- 剩余有效秒数

:::tabs

== Python

```python:line-numbers
from wingman import verification

# 默认 30 秒步长
secs = verification.remaining()
print(f"验证码还有 {secs} 秒有效")

# 自定义步长
secs = verification.remaining(60)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 默认 30 秒步长
local secs = wingman.verification.remaining()
print("验证码还有 " .. secs .. " 秒有效")

-- 自定义步长
local secs = wingman.verification.remaining(60)
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|------|
| `totp(secret, digits?, period?)` | `totp(secret, digits?, period?)` | 生成 TOTP 验证码 | secret: Base32 密钥<br>digits: 位数 (默认 6)<br>period: 步长秒数 (默认 30)<br>返回: 验证码字符串 |
| `steam_guard(secret)` | `steamGuard(secret)` | 生成 Steam Guard 验证码 | secret: 密钥<br>返回: 5 位字母验证码 |
| `verify(secret, code, digits?, period?, window?)` | `verify(secret, code, digits?, period?, window?)` | 验证 TOTP 验证码 | secret: 密钥<br>code: 验证码<br>window: 容错窗口 (默认 1)<br>返回: 是否有效 |
| `remaining(period?)` | `remaining(period?)` | 剩余有效秒数 | period: 步长秒数 (默认 30)<br>返回: 剩余秒数 |
