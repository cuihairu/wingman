# API: wingman.verification

验证码识别与 TOTP（双因素认证）模块。

## 验证码识别

:::tabs

== Python

```python
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

```lua
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

## 识别屏幕区域验证码

:::tabs

== Python

```python
from wingman import verification, screen

# 截取屏幕区域
img = screen.capture(100, 100, 200, 50)

# 识别验证码
result = verification.recognize_captcha_from_image(img)
if result['success']:
    print(f"验证码: {result['text']}")
```

== Lua

```lua
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

## TOTP 生成

:::tabs

== Python

```python
from wingman import verification

# 生成 TOTP 验证码
secret = "JBSWY3DPEHPK3PXP"
code = verification.generate_totp(secret)
print(f"当前 TOTP 验证码: {code}")
```

== Lua

```lua
local verification = require("wingman.verification")

-- 生成 TOTP 验证码
local secret = "JBSWY3DPEHPK3PXP"
local code = verification.generateTotp(secret)
print("当前 TOTP 验证码: " .. code)
```

:::

## 批量生成 TOTP

:::tabs

== Python

```python
from wingman import verification

# 批量生成多个账户的 TOTP
accounts = {
    "account1": "JBSWY3DPEHPK3PXP",
    "account2": "KRSXG5DSN5XW2==="
}

for name, secret in accounts.items():
    code = verification.generate_totp(secret)
    print(f"{name}: {code}")
```

== Lua

```lua
local verification = require("wingman.verification")

-- 批量生成多个账户的 TOTP
local accounts = {
    account1 = "JBSWY3DPEHPK3PXP",
    account2 = "KRSXG5DSN5XW2==="
}

for name, secret in pairs(accounts) do
    local code = verification.generateTotp(secret)
    print(name .. ": " .. code)
end
```

:::

## 设置 TOTP 时间步长

:::tabs

== Python

```python
from wingman import verification

# 设置 TOTP 时间步长（默认 30 秒）
verification.set_totp_step(30)

# 设置为 60 秒
verification.set_totp_step(60)
```

== Lua

```lua
local verification = require("wingman.verification")

-- 设置 TOTP 时间步长（默认 30 秒）
verification.setTotpStep(30)

-- 设置为 60 秒
verification.setTotpStep(60)
```

:::

## 存储验证码密钥

:::tabs

== Python

```python
from wingman import verification, kv

# 存储验证码密钥
verification.save_secret("game_account", "JBSWY3DPEHPK3PXP")

# 读取密钥
secret = verification.load_secret("game_account")

# 删除密钥
verification.delete_secret("game_account")
```

== Lua

```lua
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

## 完整示例

### 验证码识别流程

:::tabs

== Python

```python
from wingman import verification, input, screen, util

# 截取验证码区域
captcha_img = screen.capture(400, 300, 100, 40)

# 识别验证码
result = verification.recognize_captcha_from_image(captcha_img)

if result['success']:
    captcha_text = result['text']
    print(f"识别到验证码: {captcha_text}")

    # 输入验证码
    input.key_text(captcha_text)

    # 点击确认按钮
    util.sleep(500)
    input.click(500, 350)
else:
    print("验证码识别失败，尝试手动输入")
```

== Lua

```lua
local verification = require("wingman.verification")
local input = require("wingman.input")
local screen = require("wingman.screen")
local util = require("wingman.util")

-- 截取验证码区域
local captchaImg = screen.capture(400, 300, 100, 40)

-- 识别验证码
local result = verification.recognizeCaptchaFromImage(captchaImg)

if result.success then
    local captchaText = result.text
    print("识别到验证码: " .. captchaText)

    -- 输入验证码
    input.keyText(captchaText)

    -- 点击确认按钮
    util.sleep(500)
    input.click(500, 350)
else
    print("验证码识别失败，尝试手动输入")
end
```

:::

### TOTP 登录流程

:::tabs

== Python

```python
from wingman import verification, input, util

# 账户信息
username = "player123"
totp_secret = verification.load_secret("game_account")

if not totp_secret:
    print("未找到 TOTP 密钥，请先配置")
else:
    # 输入用户名
    input.key_text(username)
    util.sleep(500)

    # 输入密码
    input.key_text("password123")
    util.sleep(500)

    # 生成并输入 TOTP 验证码
    totp_code = verification.generate_totp(totp_secret)
    input.key_text(totp_code)

    # 点击登录按钮
    util.sleep(500)
    input.click(400, 350)
```

== Lua

```lua
local verification = require("wingman.verification")
local input = require("wingman.input")
local util = require("wingman.util")

-- 账户信息
local username = "player123"
local totpSecret = verification.loadSecret("game_account")

if not totpSecret then
    print("未找到 TOTP 密钥，请先配置")
else
    -- 输入用户名
    input.keyText(username)
    util.sleep(500)

    -- 输入密码
    input.keyText("password123")
    util.sleep(500)

    -- 生成并输入 TOTP 验证码
    local totpCode = verification.generateTotp(totpSecret)
    input.keyText(totpCode)

    -- 点击登录按钮
    util.sleep(500)
    input.click(400, 350)
end
```

:::

---

## 可用接口

### `recognize_captcha(image_path)` / `recognizeCaptcha(imagePath)`

识别图像验证码。

**参数：**
- `image_path` / `imagePath` - 验证码图片路径

**返回：**
- `dict/table` - `{success: boolean, text: string, confidence: number}`

### `recognize_captcha_from_image(image)` / `recognizeCaptchaFromImage(image)`

从图像对象识别验证码。

**参数：**
- `image` - 图像对象（由 screen.capture 返回）

**返回：**
- `dict/table` - `{success: boolean, text: string, confidence: number}`

### `generate_totp(secret)` / `generateTotp(secret)`

生成 TOTP 验证码。

**参数：**
- `secret` - Base32 编码的密钥

**返回：**
- `string` - 6 位数字验证码

### `set_totp_step(seconds)` / `setTotpStep(seconds)`

设置 TOTP 时间步长。

**参数：**
- `seconds` - 时间步长（秒），默认 30

### `save_secret(name, secret)` / `saveSecret(name, secret)`

存储验证码密钥。

**参数：**
- `name` - 密钥名称
- `secret` - 密钥值

### `load_secret(name)` / `loadSecret(name)`

读取验证码密钥。

**参数：**
- `name` - 密钥名称

**返回：**
- `string` - 密钥值，不存在返回 None/nil

### `delete_secret(name)` / `deleteSecret(name)`

删除验证码密钥。

**参数：**
- `name` - 密钥名称
