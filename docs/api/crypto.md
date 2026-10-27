> [中文](../zh/api/crypto.md) | **English**

# Crypto

This document is lengthy (437 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [crypto.md](../zh/api/crypto.md) for the original.

- 设计约束
- 函数列表
- AES 对称加密
  - crypto.encryptAES(plaintext, password, salt?)
  - crypto.decryptAES(ciphertext, password)
- 密钥派生
  - crypto.deriveKey(password, salt, iterations?, keyLen?)
  - crypto.generateSalt(length?)
- 哈希
  - crypto.sha256(data)
  - crypto.sha512(data)
- Base64 编码
  - crypto.base64Encode(data)
  - crypto.base64Decode(base64_string)
- Hex 编码
  - crypto.hexEncode(data)
  - crypto.hexDecode(hex_string)
- 随机字节
  - crypto.randomBytes(length?)
- 完整示例
  - Python
  - Lua
- 安全注意事项
