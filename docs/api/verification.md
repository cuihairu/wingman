> [中文](../zh/api/verification.md) | **English**

# API: wingman.verification

A pure utility function module for TOTP (two-factor authentication).

## Module Overview

The verification module provides stateless TOTP utility functions:
- **TOTP generation** - generate time-based one-time codes
- **Steam Guard** - generate Steam Guard codes
- **Code verification** - verify whether a TOTP code is valid
- **Remaining time** - get the remaining valid seconds of the current code

> All functions are stateless pure functions and do not involve key persistence. If you need to store keys, use the `wingman.kv` module.

---

## Generate a TOTP Code

### totp(secret, digits?, period?) / totp(secret, digits?, period?)

**Description**: Generates a time-based one-time code (TOTP, RFC 6238).

**Function signature**:

```python
totp(secret: str, digits: int = 6, period: int = 30) -> str
```

```lua
totp(secret: string, digits: number?, period: number?) -> string
```

**Parameters**:
- `secret` - Base32 encoded key. **Must be ≥16 characters and contain only Base32 characters (`A-Z`, `2-7`, spaces allowed)**; otherwise an empty string is returned
- `digits` - number of code digits, default 6
- `period` - time step in seconds, default 30

**Returns**:
- A numeric code string

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

## Generate a Steam Guard Code

### steam_guard(secret) / steamGuard(secret)

**Description**: Generates a Steam Guard code (5 digits).

**Function signature**:

```python
steam_guard(secret: str) -> str
```

```lua
steamGuard(secret: string) -> string
```

**Parameters**:
- `secret` - Steam Guard key (Base32 of ≥16 characters, or a Base64 encoded key)

**Returns**:
- A 5-digit **numeric** code (00000-99999, zero-padded)

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

## Verify a TOTP Code

### verify(secret, code, digits?, period?, window?) / verify(secret, code, digits?, period?, window?)

**Description**: Verifies whether a TOTP code is valid. Supports a time-window tolerance.

**Function signature**:

```python
verify(secret: str, code: str, digits: int = 6, period: int = 30, window: int = 1) -> bool
```

```lua
verify(secret: string, code: string, digits: number?, period: number?, window: number?) -> boolean
```

**Parameters**:
- `secret` - Base32 encoded key. **Must be ≥16 characters and contain only Base32 characters (`A-Z`, `2-7`, spaces allowed)**; otherwise `False`/`false` is returned
- `code` - the code to verify
- `digits` - number of code digits, default 6
- `period` - time step in seconds, default 30
- `window` - time-window tolerance count, default 1 (allows 1 period before/after)

**Returns**:
- Whether the code is valid

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

## Get the Remaining Valid Time

### remaining(period?) / remaining(period?)

**Description**: Gets the remaining valid seconds of the current TOTP code.

**Function signature**:

```python
remaining(period: int = 30) -> int
```

```lua
remaining(period: number?) -> number
```

**Parameters**:
- `period` - time step in seconds, default 30

**Returns**:
- Remaining valid seconds

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

## Available Interfaces

| Python function | Lua function | Description | Parameters |
|------------|---------|------|------|
| `totp(secret, digits?, period?)` | `totp(secret, digits?, period?)` | Generate a TOTP code | secret: Base32 key (≥16 chars)<br>digits: digit count (default 6)<br>period: step seconds (default 30)<br>Return: code string |
| `steam_guard(secret)` | `steamGuard(secret)` | Generate a Steam Guard code | secret: key<br>Return: 5-digit numeric code |
| `verify(secret, code, digits?, period?, window?)` | `verify(secret, code, digits?, period?, window?)` | Verify a TOTP code | secret: key (≥16 chars Base32)<br>code: code<br>window: tolerance window (default 1)<br>Return: whether valid |
| `remaining(period?)` | `remaining(period?)` | Remaining valid seconds | period: step seconds (default 30)<br>Return: remaining seconds |
