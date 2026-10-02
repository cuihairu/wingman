package security

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/pbkdf2"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
)

// 保险箱加密原语（models/vault.go 头注释里的结构在此落地）。
// 纯函数、零状态：解锁驻留与空闲锁回在 handlers/vault.go。

// VaultKDFIterations PBKDF2-HMAC-SHA256 轮数（OWASP 2023 对该组合的
// 建议值 600k）。只在解锁时执行一次，解锁延迟可接受。
const VaultKDFIterations = 600_000

// ErrVaultWrongPassword 主口令错误 / 密文被篡改（GCM 认证失败，两者
// 在 AEAD 下不可区分——这也是刻意的：不给攻击者分辨「盐对不对」的通道）
var ErrVaultWrongPassword = errors.New("vault: wrong master password or corrupted data")

// VaultDeriveKey 主口令派生 KEK（PBKDF2-HMAC-SHA256）。
func VaultDeriveKey(masterPassword, saltHex string, iterations int) ([]byte, error) {
	salt, err := hex.DecodeString(saltHex)
	if err != nil || len(salt) < 16 {
		return nil, fmt.Errorf("vault: bad kdf salt")
	}
	if iterations < 1 {
		return nil, fmt.Errorf("vault: bad kdf iterations")
	}
	return pbkdf2.Key(sha256.New, masterPassword, salt, iterations, 32)
}

// VaultGenerateSalt 新 KDF 盐（16 字节 hex）。
func VaultGenerateSalt() (string, error) {
	b := make([]byte, 16)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}

// VaultGenerateDEK 随机 32 字节数据密钥。
func VaultGenerateDEK() ([]byte, error) {
	dek := make([]byte, 32)
	if _, err := rand.Read(dek); err != nil {
		return nil, err
	}
	return dek, nil
}

// vaultSeal AES-256-GCM 加密 → hex(nonce‖ciphertext)。
func vaultSeal(key []byte, plaintext []byte) (string, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return "", err
	}
	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}
	nonce := make([]byte, gcm.NonceSize())
	if _, err := rand.Read(nonce); err != nil {
		return "", err
	}
	out := gcm.Seal(nonce, nonce, plaintext, nil)
	return hex.EncodeToString(out), nil
}

// vaultOpen AES-256-GCM 解密 hex(nonce‖ciphertext)。认证失败统一折算为
// ErrVaultWrongPassword（见该错误注释）。
func vaultOpen(key []byte, sealedHex string) ([]byte, error) {
	raw, err := hex.DecodeString(sealedHex)
	if err != nil {
		return nil, ErrVaultWrongPassword
	}
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}
	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}
	if len(raw) < gcm.NonceSize() {
		return nil, ErrVaultWrongPassword
	}
	nonce, ct := raw[:gcm.NonceSize()], raw[gcm.NonceSize():]
	out, err := gcm.Open(nil, nonce, ct, nil)
	if err != nil {
		return nil, ErrVaultWrongPassword
	}
	return out, nil
}

// VaultWrapKey 用 KEK 包裹 DEK（落库形态）。
func VaultWrapKey(kek []byte, dek []byte) (string, error) {
	return vaultSeal(kek, dek)
}

// VaultUnwrapKey 解开 DEK；主口令错误 → ErrVaultWrongPassword。
func VaultUnwrapKey(kek []byte, wrappedHex string) ([]byte, error) {
	return vaultOpen(kek, wrappedHex)
}

// VaultEncryptString 用 DEK 加密一条凭据字符串（落库形态）。
func VaultEncryptString(dek []byte, plaintext string) (string, error) {
	return vaultSeal(dek, []byte(plaintext))
}

// VaultDecryptString 解密一条凭据；失败统一 ErrVaultWrongPassword。
func VaultDecryptString(dek []byte, sealedHex string) (string, error) {
	out, err := vaultOpen(dek, sealedHex)
	if err != nil {
		return "", err
	}
	return string(out), nil
}
