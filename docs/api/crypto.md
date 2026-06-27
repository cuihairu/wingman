# API: wingman.crypto

crypto 模块提供密码学原语，包括 AES-256-GCM 对称加解密、密钥派生（PBKDF2）、盐值/随机字节生成、哈希、以及 base64 / hex 编码。

## 设计约束

1. **本地密码学原语**
   - crypto 模块是脚本本地使用的密码学工具集，不参与 orchestrator/server 的密钥管理或鉴权。
   - 模块不持久化任何密钥或密文；调用方需自行决定如何存储（如配合 `db` / `kv` 模块）。

2. **加密算法**
   - `encryptAES` / `decryptAES` 使用 AES-256-GCM。
   - 密文以 base64 字符串形式返回，IV 内嵌于密文中。

3. **密钥派生**
   - `deriveKey` 基于 PBKDF2-HMAC，默认迭代次数 100000、默认输出 32 字节密钥。
   - 盐值以十六进制字符串形式传入（可通过 `generateSalt` 生成）。

4. **参数校验**
   - 必填参数缺失或类型不符时，函数返回空字符串（`""`）并记录错误日志，不抛出异常。
   - 调用方应自行检查返回值是否为空。

5. **字节序列处理**
   - `base64Encode` / `hexEncode` 接收字符串，按字节逐个编码。
   - `randomBytes` 返回原始字节字符串（可能包含不可打印字符），如需可打印形式请配合 `hexEncode` / `base64Encode` 使用。

## 函数列表

crypto 模块在 C++ 侧以 `mod.name = "crypto"` 注册，共 11 个函数：

| 函数 | 签名 |
|------|------|
| `encryptAES` | `(plaintext, password, salt?) -> base64_string` |
| `decryptAES` | `(ciphertext, password) -> plaintext` |
| `deriveKey` | `(password, salt, iterations?, keyLen?) -> hex_string` |
| `generateSalt` | `(length?) -> hex_string` |
| `sha256` | `(data) -> hex_string` |
| `sha512` | `(data) -> hex_string` |
| `base64Encode` | `(data) -> base64_string` |
| `base64Decode` | `(base64_string) -> data` |
| `hexEncode` | `(data) -> hex_string` |
| `hexDecode` | `(hex_string) -> data` |
| `randomBytes` | `(length?) -> bytes_string` |

---

## AES 对称加密

### crypto.encryptAES(plaintext, password, salt?)

使用 AES-256-GCM 加密明文。

**参数：**
- `plaintext` (string): 待加密的明文
- `password` (string): 加密口令
- `salt` (string?, optional): 盐值，用于派生密钥；不传时由实现内部处理

**返回：**
- string: base64 编码的密文（含 IV）；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

ciphertext = crypto.encryptAES("hello world", "my-password", "a1b2c3d4e5f6")
print(ciphertext)  # base64 密文
```

```lua
local wingman = require("wingman")

local ciphertext = wingman.crypto.encryptAES("hello world", "my-password", "a1b2c3d4e5f6")
print(ciphertext)  -- base64 密文
```

### crypto.decryptAES(ciphertext, password)

使用 AES-256-GCM 解密密文。

**参数：**
- `ciphertext` (string): base64 编码的密文（由 `encryptAES` 生成）
- `password` (string): 解密口令（需与加密时一致）

**返回：**
- string: 还原后的明文；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

ciphertext = crypto.encryptAES("secret", "pw")
plaintext = crypto.decryptAES(ciphertext, "pw")
print(plaintext)  # "secret"
```

```lua
local wingman = require("wingman")

local ciphertext = wingman.crypto.encryptAES("secret", "pw")
local plaintext = wingman.crypto.decryptAES(ciphertext, "pw")
print(plaintext)  -- "secret"
```

---

## 密钥派生

### crypto.deriveKey(password, salt, iterations?, keyLen?)

使用 PBKDF2 从口令派生密钥。

**参数：**
- `password` (string): 口令
- `salt` (string): 盐值（十六进制字符串，建议用 `generateSalt` 生成）
- `iterations` (int?, optional): 迭代次数，默认 `100000`
- `keyLen` (int?, optional): 输出密钥字节数，默认 `32`

**返回：**
- string: 派生密钥（十六进制字符串）；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

salt = crypto.generateSalt(16)
key = crypto.deriveKey("my-password", salt)            # 使用默认参数
key2 = crypto.deriveKey("my-password", salt, 200000, 64)  # 自定义迭代与长度
print(key)
```

```lua
local wingman = require("wingman")

local salt = wingman.crypto.generateSalt(16)
local key = wingman.crypto.deriveKey("my-password", salt)
local key2 = wingman.crypto.deriveKey("my-password", salt, 200000, 64)
print(key)
```

### crypto.generateSalt(length?)

生成随机盐值。

**参数：**
- `length` (int?, optional): 盐值字节数，默认 `16`

**返回：**
- string: 十六进制盐值字符串

**示例：**

```python
from wingman import crypto

salt = crypto.generateSalt()    # 默认 16 字节
salt32 = crypto.generateSalt(32)
```

```lua
local wingman = require("wingman")

local salt = wingman.crypto.generateSalt()
local salt32 = wingman.crypto.generateSalt(32)
```

---

## 哈希

### crypto.sha256(data)

计算 SHA-256 哈希。

**参数：**
- `data` (string): 待哈希的数据

**返回：**
- string: 十六进制哈希值；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

digest = crypto.sha256("hello world")
print(digest)  # b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
```

```lua
local wingman = require("wingman")

local digest = wingman.crypto.sha256("hello world")
print(digest)
```

### crypto.sha512(data)

计算 SHA-512 哈希。

**参数：**
- `data` (string): 待哈希的数据

**返回：**
- string: 十六进制哈希值；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

digest = crypto.sha512("hello world")
print(digest)
```

```lua
local wingman = require("wingman")

local digest = wingman.crypto.sha512("hello world")
print(digest)
```

---

## Base64 编码

### crypto.base64Encode(data)

将字符串编码为 base64。

**参数：**
- `data` (string): 待编码的数据（按字节处理）

**返回：**
- string: base64 编码字符串；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

encoded = crypto.base64Encode("hello world")  # "aGVsbG8gd29ybGQ="
```

```lua
local wingman = require("wingman")

local encoded = wingman.crypto.base64Encode("hello world")  -- "aGVsbG8gd29ybGQ="
```

### crypto.base64Decode(base64_string)

将 base64 字符串解码。

**参数：**
- `base64_string` (string): base64 编码字符串

**返回：**
- string: 解码后的原始数据；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

decoded = crypto.base64Decode("aGVsbG8gd29ybGQ=")  # "hello world"
```

```lua
local wingman = require("wingman")

local decoded = wingman.crypto.base64Decode("aGVsbG8gd29ybGQ=")  -- "hello world"
```

---

## Hex 编码

### crypto.hexEncode(data)

将字符串编码为十六进制。

**参数：**
- `data` (string): 待编码的数据（按字节处理）

**返回：**
- string: 十六进制编码字符串；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

encoded = crypto.hexEncode("AB")  # "4142"
```

```lua
local wingman = require("wingman")

local encoded = wingman.crypto.hexEncode("AB")  -- "4142"
```

### crypto.hexDecode(hex_string)

将十六进制字符串解码。

**参数：**
- `hex_string` (string): 十六进制编码字符串

**返回：**
- string: 解码后的原始数据；参数缺失时返回 `""`

**示例：**

```python
from wingman import crypto

decoded = crypto.hexDecode("4142")  # "AB"
```

```lua
local wingman = require("wingman")

local decoded = wingman.crypto.hexDecode("4142")  -- "AB"
```

---

## 随机字节

### crypto.randomBytes(length?)

生成加密安全的随机字节。

**参数：**
- `length` (int?, optional): 字节数，默认 `16`

**返回：**
- string: 原始字节字符串（可能含不可打印字符），建议配合 `hexEncode` 或 `base64Encode` 转可打印形式

**示例：**

```python
from wingman import crypto

raw = crypto.randomBytes(32)            # 原始字节
token = crypto.hexEncode(crypto.randomBytes(32))  # 可打印十六进制 token
```

```lua
local wingman = require("wingman")

local raw = wingman.crypto.randomBytes(32)
local token = wingman.crypto.hexEncode(wingman.crypto.randomBytes(32))  -- 可打印 token
```

---

## 完整示例

### Python

```python
from wingman import crypto

# 1. AES 加解密
salt = crypto.generateSalt(16)
ciphertext = crypto.encryptAES("敏感数据", "strong-password", salt)
plaintext = crypto.decryptAES(ciphertext, "strong-password")
print(plaintext)  # "敏感数据"

# 2. 密钥派生
key = crypto.deriveKey("strong-password", salt, 100000, 32)
print(f"derived key: {key}")

# 3. 哈希校验
assert crypto.sha256("hello") == crypto.sha256("hello")

# 4. 随机 token 生成
token = crypto.hexEncode(crypto.randomBytes(32))
print(f"token: {token}")

# 5. 编码往返
assert crypto.base64Decode(crypto.base64Encode("wingman")) == "wingman"
assert crypto.hexDecode(crypto.hexEncode("wingman")) == "wingman"
```

### Lua

```lua
local wingman = require("wingman")
local crypto = wingman.crypto

-- 1. AES 加解密
local salt = crypto.generateSalt(16)
local ciphertext = crypto.encryptAES("敏感数据", "strong-password", salt)
local plaintext = crypto.decryptAES(ciphertext, "strong-password")
print(plaintext)  -- "敏感数据"

-- 2. 密钥派生
local key = crypto.deriveKey("strong-password", salt, 100000, 32)
print("derived key: " .. key)

-- 3. 哈希校验
assert(crypto.sha256("hello") == crypto.sha256("hello"))

-- 4. 随机 token 生成
local token = crypto.hexEncode(crypto.randomBytes(32))
print("token: " .. token)

-- 5. 编码往返
assert(crypto.base64Decode(crypto.base64Encode("wingman")) == "wingman")
assert(crypto.hexDecode(crypto.hexEncode("wingman")) == "wingman")
```

## 安全注意事项

1. **口令管理**
   - 加密口令不会被模块存储；调用方需自行安全管理。
   - 建议使用 `deriveKey` 生成强密钥，而非直接使用短口令。

2. **盐值复用**
   - 每次派生密钥应使用独立盐值（通过 `generateSalt` 生成），避免固定盐。
   - 盐值需与密文一同持久化，否则无法还原。

3. **随机数来源**
   - `randomBytes` / `generateSalt` 使用加密安全的随机源，适合密钥、token、盐值生成。

4. **不可逆哈希**
   - `sha256` / `sha512` 为不可逆哈希，不适合密码存储；如需存储口令请使用 `deriveKey` 配合高迭代次数。

5. **返回空串**
   - 参数错误时返回 `""` 而非抛异常，调用方应显式检查，避免把空串当作有效密文/明文使用。
