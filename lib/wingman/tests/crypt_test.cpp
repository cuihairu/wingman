#include <gtest/gtest.h>
#include "wingman/crypt.hpp"
#include <string>
#include <vector>

using namespace wingman::crypt;

// ========== Base64 ==========

TEST(CryptBase64Test, EncodeEmpty) {
    EXPECT_EQ(base64Encode({}), "");
}

TEST(CryptBase64Test, EncodeKnownVectors) {
    // RFC 4648 测试向量
    EXPECT_EQ(base64Encode({'f'}), "Zg==");
    EXPECT_EQ(base64Encode({'f', 'o'}), "Zm8=");
    EXPECT_EQ(base64Encode({'f', 'o', 'o'}), "Zm9v");
    EXPECT_EQ(base64Encode({'f', 'o', 'o', 'b'}), "Zm9vYg==");
    EXPECT_EQ(base64Encode({'f', 'o', 'o', 'b', 'a'}), "Zm9vYmE=");
    EXPECT_EQ(base64Encode({'f', 'o', 'o', 'b', 'a', 'r'}), "Zm9vYmFy");
}

TEST(CryptBase64Test, DecodeRoundTrip) {
    const std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0xFE, 0xFF, 0x7F, 0x80};
    auto decoded = base64Decode(base64Encode(data));
    EXPECT_EQ(decoded, data);
}

TEST(CryptBase64Test, DecodeEmptyAndPadding) {
    EXPECT_TRUE(base64Decode("").empty());
    EXPECT_EQ(base64Decode("Zg=="), std::vector<uint8_t>({'f'}));
}

TEST(CryptBase64Test, DecodeSkipsInvalidCharacters) {
    // 非法字符应被跳过，'=' 终止解码
    auto decoded = base64Decode("Zm9v\n\r Yg==");
    EXPECT_EQ(decoded, std::vector<uint8_t>({'f', 'o', 'o', 'b'}));
}

// ========== Hex ==========

TEST(CryptHexTest, BytesToHex) {
    EXPECT_EQ(bytesToHex({0x00, 0x1a, 0xff}), "001aff");
    EXPECT_EQ(bytesToHex({}), "");
}

TEST(CryptHexTest, HexToBytes) {
    EXPECT_EQ(hexToBytes("001aff"), (std::vector<uint8_t>{0x00, 0x1a, 0xff}));
    EXPECT_TRUE(hexToBytes("").empty());
}

TEST(CryptHexTest, HexRoundTrip) {
    const std::vector<uint8_t> bytes = {0x00, 0x10, 0xab, 0xcd, 0xef, 0xff};
    EXPECT_EQ(hexToBytes(bytesToHex(bytes)), bytes);
}

// ========== Random ==========

TEST(CryptRandomTest, RandomBytesLength) {
    EXPECT_EQ(randomBytes(0).size(), 0u);
    EXPECT_EQ(randomBytes(32).size(), 32u);
    EXPECT_EQ(randomBytes(1024).size(), 1024u);
}

TEST(CryptRandomTest, RandomBytesAreRandom) {
    auto a = randomBytes(64);
    auto b = randomBytes(64);
    EXPECT_NE(a, b);
}

TEST(CryptRandomTest, GenerateSalt) {
    auto salt = generateSalt(16);
    EXPECT_EQ(salt.size(), 32u);  // 16 字节 = 32 个十六进制字符
    // 合法十六进制
    EXPECT_FALSE(salt.empty());
    auto bytes = hexToBytes(salt);
    EXPECT_EQ(bytes.size(), 16u);
}

// ========== SHA ==========

TEST(CryptShaTest, Sha256KnownVector) {
    // SHA-256("abc")
    EXPECT_EQ(sha256("abc"),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256(""),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(CryptShaTest, Sha512KnownVector) {
    // SHA-512("abc")
    EXPECT_EQ(sha512("abc"),
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    EXPECT_EQ(sha512(""),
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
        "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
}

// ========== Key Derivation ==========

TEST(CryptDeriveKeyTest, DeriveKeyDeterministic) {
    auto key1 = deriveKey("password", "0123456789abcdef", 1000);
    auto key2 = deriveKey("password", "0123456789abcdef", 1000);
    EXPECT_EQ(key1, key2);
    EXPECT_EQ(key1.size(), 64u);  // 32 字节 key = 64 hex 字符
}

TEST(CryptDeriveKeyTest, DeriveKeyDiffersByPassword) {
    auto key1 = deriveKey("password1", "0123456789abcdef", 1000);
    auto key2 = deriveKey("password2", "0123456789abcdef", 1000);
    EXPECT_NE(key1, key2);
}

TEST(CryptDeriveKeyTest, DeriveKeyCustomLength) {
    auto key = deriveKey("password", "0123456789abcdef", 1000, 16);
    EXPECT_EQ(key.size(), 32u);
}

TEST(CryptDeriveKeyTest, DeriveKeyInvalidSalt) {
    EXPECT_EQ(deriveKey("password", "", 1000), "");
}

// ========== AES-256-GCM ==========

TEST(CryptAesTest, EncryptDecryptRoundTrip) {
    const std::string plaintext = "Hello, Wingman!";
    const std::string password = "secret-password";

    auto encrypted = encryptAES(plaintext, password);
    ASSERT_FALSE(encrypted.empty());

    auto decrypted = decryptAES(encrypted, password);
    EXPECT_EQ(decrypted, plaintext);
}

TEST(CryptAesTest, EncryptDecryptEmptyPlaintext) {
    auto encrypted = encryptAES("", "password");
    ASSERT_FALSE(encrypted.empty());
    EXPECT_EQ(decryptAES(encrypted, "password"), "");
}

TEST(CryptAesTest, EncryptWithExplicitSaltRoundTrip) {
    auto salt = generateSalt(16);
    auto encrypted = encryptAES("data", "password", salt);
    ASSERT_FALSE(encrypted.empty());
    EXPECT_EQ(decryptAES(encrypted, "password"), "data");
}

TEST(CryptAesTest, EncryptInvalidSaltFails) {
    // salt 长度不是 16 字节（32 hex 字符）
    EXPECT_EQ(encryptAES("data", "password", "abcd"), "");
}

TEST(CryptAesTest, RandomIVProducesDifferentCiphertext) {
    auto c1 = encryptAES("same plaintext", "password");
    auto c2 = encryptAES("same plaintext", "password");
    ASSERT_FALSE(c1.empty());
    ASSERT_FALSE(c2.empty());
    EXPECT_NE(c1, c2);  // 随机 IV 应使每次密文不同
}

TEST(CryptAesTest, DecryptWrongPasswordFails) {
    auto encrypted = encryptAES("secret", "correct-password");
    ASSERT_FALSE(encrypted.empty());
    EXPECT_EQ(decryptAES(encrypted, "wrong-password"), "");
}

TEST(CryptAesTest, DecryptInvalidInputFails) {
    EXPECT_EQ(decryptAES("", "password"), "");
    EXPECT_EQ(decryptAES("not-base64!!!", "password"), "");
    // 太短：小于 salt(16)+iv(12)+tag(16)
    EXPECT_EQ(decryptAES(base64Encode(std::vector<uint8_t>(10, 0x00)), "password"), "");
}

TEST(CryptAesTest, DecryptTamperedCiphertextFails) {
    auto encrypted = encryptAES("secret data for tamper test", "password");
    ASSERT_FALSE(encrypted.empty());

    auto raw = base64Decode(encrypted);
    ASSERT_GE(raw.size(), 44u + 1);
    // 篡改密文部分（salt+iv 之后）
    raw[28] ^= 0xFF;
    auto tampered = base64Encode(raw);

    EXPECT_EQ(decryptAES(tampered, "password"), "");
}

TEST(CryptAesTest, LargeDataRoundTrip) {
    const std::string large(10000, 'x');
    auto encrypted = encryptAES(large, "password");
    ASSERT_FALSE(encrypted.empty());
    EXPECT_EQ(decryptAES(encrypted, "password"), large);
}
