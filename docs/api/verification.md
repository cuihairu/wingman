# API: wingman.verification

验证码识别与 TOTP（双因素认证）模块。

## 模块概述

verification 模块提供验证功能：
- **验证码识别** - 识别图像验证码、屏幕区域验证码
- **TOTP 生成** - 生成 TOTP 双因素认证验证码
- **密钥管理** - 存储、读取、删除 TOTP 密钥

---

## 识别图像验证码

### recognize_captcha(image_path) / recognizeCaptcha(imagePath)

**说明**：识别图像验证码。

**函数签名**：

```python
recognize_captcha(image_path: str) -> dict
```

```lua
recognizeCaptcha(imagePath: string) -> table
```

**参数**：
- `image_path` / `imagePath` - 验证码图片路径

**返回**：
- 识别结果对象：
  - `success` / `success` - 是否成功
  - `text` / `text` - 识别的文本
  - `confidence` / `confidence` - 置信度（0-1）

:::tabs

== Python

```python:line-numbers
from wingman import verification

# 识别图像验证码
result = verification.recognize_captcha("captcha.png")
if result['success']:
    print(f"验证码: {result['text']}")
    print(f"置信度: {result['confidence']}")
else:
    print("识别失败")
```

== Lua

```lua:line-numbers
local verification = require("wingman.verification")

-- 识别图像验证码
local result = verification.recognizeCaptcha("captcha.png")
if result.success then
    print("验证码: " .. result.text)
    print("置信度: " .. result.confidence)
else
    print("识别失败")
end
```

:::

---

## 识别屏幕区域验证码

### recognize_captcha_from_image(image) / recognizeCaptchaFromImage(image)

**说明**：从图像对象识别验证码。

**函数签名**：

```python
recognize_captcha_from_image(image) -> dict
```

```lua
recognizeCaptchaFromImage(image) -> table
```

**参数**：
- `image` - 图像对象（由 `screen.capture()` 返回）

**返回**：
- 识别结果对象：
  - `success` / `success` - 是否成功
  - `text` / `text` - 识别的文本
  - `confidence` / `confidence` - 置信度（0-1）

:::tabs

== Python

```python:line-numbers
from wingman import verification, screen

# 截取屏幕区域
img = screen.capture(100, 100, 200, 50)

# 识别验证码
result = verification.recognize_captcha_from_image(img)
if result['success']:
    print(f"验证码: {result['text']}")
```

== Lua

```lua:line-numbers
local verification = require("wingman.verification")
local screen = require("wingman.screen")

-- 截取屏幕区域
local img = screen.capture(100, 100, 200, 50)

-- 识别验证码
local result = verification.recognizeCaptchaFromImage(img)
if result.success then
    print("验证码: " .. result.text)
end
```

:::

---

## 生成 TOTP 验证码

### generate_totp(secret) / generateTotp(secret)

**说明**：生成 TOTP 验证码。

**函数签名**：

```python
generate_totp(secret: str) -> str
```

```lua
generateTotp(secret: string) -> string
```

**参数**：
- `secret` - Base32 编码的密钥

**返回**：
- 6 位数字验证码

:::tabs

== Python

```python:line-numbers
from wingman import verification

# 生成 TOTP 验证码
secret = "JBSWY3DPEHPK3PXP"
code = verification.generate_totp(secret)
print(f"当前 TOTP 验证码: {code}")
```

== Lua

```lua:line-numbers
local verification = require("wingman.verification")

-- 生成 TOTP 验证码
local secret = "JBSWY3DPEHPK3PXP"
local code = verification.generateTotp(secret)
print("当前 TOTP 验证码: " .. code)
```

:::

---

## 设置 TOTP 时间步长

### set_totp_step(seconds) / setTotpStep(seconds)

**说明**：设置 TOTP 时间步长。

**函数签名**：

```python
set_totp_step(seconds: int) -> None
```

```lua
setTotpStep(seconds: number) -> nil
```

**参数**：
- `seconds` - 时间步长（秒），默认 30

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import verification

# 设置 TOTP 时间步长（默认 30 秒）
verification.set_totp_step(30)

# 设置为 60 秒
verification.set_totp_step(60)
```

== Lua

```lua:line-numbers
local verification = require("wingman.verification")

-- 设置 TOTP 时间步长（默认 30 秒）
verification.setTotpStep(30)

-- 设置为 60 秒
verification.setTotpStep(60)
```

:::

---

## 存储验证码密钥

### save_secret(name, secret) / saveSecret(name, secret)

**说明**：存储验证码密钥。

**函数签名**：

```python
save_secret(name: str, secret: str) -> None
```

```lua
saveSecret(name: string, secret: string) -> nil
```

**参数**：
- `name` - 密钥名称
- `secret` - 密钥值

**返回**：
- 无

---

## 读取验证码密钥

### load_secret(name) / loadSecret(name)

**说明**：读取验证码密钥。

**函数签名**：

```python
load_secret(name: str) -> str | None
```

```lua
loadSecret(name: string) -> string | nil
```

**参数**：
- `name` - 密钥名称

**返回**：
- Python: 密钥值，不存在返回 `None`
- Lua: 密钥值，不存在返回 `nil`

---

## 删除验证码密钥

### delete_secret(name) / deleteSecret(name)

**说明**：删除验证码密钥。

**函数签名**：

```python
delete_secret(name: str) -> None
```

```lua
deleteSecret(name: string) -> nil
```

**参数**：
- `name` - 密钥名称

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import verification

# 存储验证码密钥
verification.save_secret("game_account", "JBSWY3DPEHPK3PXP")

# 读取密钥
secret = verification.load_secret("game_account")

# 删除密钥
verification.delete_secret("game_account")
```

== Lua

```lua:line-numbers
local verification = require("wingman.verification")

-- 存储验证码密钥
verification.saveSecret("game_account", "JBSWY3DPEHPK3PXP")

-- 读取密钥
local secret = verification.loadSecret("game_account")

-- 删除密钥
verification.deleteSecret("game_account")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `recognize_captcha(imagePath)` | `recognizeCaptcha(imagePath)` | 识别图像验证码 | imagePath: 图片路径<br>返回: 结果对象 |
| `recognize_captcha_from_image(image)` | `recognizeCaptchaFromImage(image)` | 识别屏幕验证码 | image: 图像对象<br>返回: 结果对象 |
| `generate_totp(secret)` | `generateTotp(secret)` | 生成TOTP验证码 | secret: Base32密钥<br>返回: 6位验证码 |
| `set_totp_step(seconds)` | `setTotpStep(seconds)` | 设置时间步长 | seconds: 秒数 |
| `save_secret(name, secret)` | `saveSecret(name, secret)` | 存储密钥 | name: 密钥名称<br>secret: 密钥值 |
| `load_secret(name)` | `loadSecret(name)` | 读取密钥 | name: 密钥名称<br>返回: 密钥值或None/nil |
| `delete_secret(name)` | `deleteSecret(name)` | 删除密钥 | name: 密钥名称 |

---

## 返回值结构

### 验证码识别结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | boolean | 是否识别成功 |
| `text` | string | 识别的文本内容 |
| `confidence` | number | 识别置信度（0-1） |
