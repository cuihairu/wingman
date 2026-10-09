#pragma once

// HMAC-SHA256 / SHA-256（A3-P2 challenge-response，docs/agent-token-auth-design.md
// §6.1）。自含实现而非引 OpenSSL：agentcore 只需一次 HMAC 调用，自含零新增
// 依赖（Android NDK 构建免补 openssl 包），且单测可用 RFC 4231 标准向量验证。
//
// 与 Go server 侧 tokenMAC（internal/agent/listener.go）的字节语义严格一致：
//   mac = HMAC-SHA256(keyHex 的 ASCII 字节, messageHex 的 ASCII 字节)
// challenge 应答直接使用：key = sha256Hex(token) 的 ASCII 字节、
// message = nonce 的 ASCII 字节。

#include <cstdint>
#include <string>

namespace wingman::runtime {

// SHA-256 摘要（原始字节入、32 字节原始字节出）。
auto sha256(const std::string& message) -> std::string;

// SHA-256 摘要（hex 小写出）。
auto sha256Hex(const std::string& message) -> std::string;

// HMAC-SHA256（原始字节入、32 字节原始字节出；RFC 2104 标准 ipad/opad）。
auto hmacSha256(const std::string& key, const std::string& message) -> std::string;

// HMAC-SHA256（hex 小写出）：key/message 均按 hex 串的 ASCII 字节参与运算——
// 与 server 侧 tokenMAC 字节语义一致，challenge 应答直接使用：
//   hmacSha256HexKey(sha256Hex(token), nonceHex)
auto hmacSha256HexKey(const std::string& keyHex, const std::string& messageHex) -> std::string;

} // namespace wingman::runtime
