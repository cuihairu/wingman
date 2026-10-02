package security

import (
	"bytes"
	"errors"
	"testing"
)

// ---------- 主口令派生（PBKDF2） ----------

func TestVaultDeriveKeyDeterministicAndSalted(t *testing.T) {
	salt, err := VaultGenerateSalt()
	if err != nil {
		t.Fatalf("generate salt: %v", err)
	}
	k1, err := VaultDeriveKey("master-pass", salt, VaultKDFIterations)
	if err != nil {
		t.Fatalf("derive: %v", err)
	}
	k2, err := VaultDeriveKey("master-pass", salt, VaultKDFIterations)
	if err != nil || !bytes.Equal(k1, k2) {
		t.Fatalf("same input must derive same key")
	}
	if len(k1) != 32 {
		t.Fatalf("key length = %d, want 32 (AES-256)", len(k1))
	}
	salt2, _ := VaultGenerateSalt()
	k3, _ := VaultDeriveKey("master-pass", salt2, VaultKDFIterations)
	if bytes.Equal(k1, k3) {
		t.Fatalf("different salt must derive different key")
	}
	k4, _ := VaultDeriveKey("master-pasS", salt, VaultKDFIterations)
	if bytes.Equal(k1, k4) {
		t.Fatalf("different password must derive different key")
	}
	// 轮数参与派生（落库轮数自洽性的前提）
	k5, _ := VaultDeriveKey("master-pass", salt, VaultKDFIterations+1000)
	if bytes.Equal(k1, k5) {
		t.Fatalf("different iterations must derive different key")
	}
}

func TestVaultDeriveKeyRejectsBadSalt(t *testing.T) {
	if _, err := VaultDeriveKey("p", "zz-not-hex", 1000); err == nil {
		t.Fatalf("non-hex salt should be rejected")
	}
	if _, err := VaultDeriveKey("p", "aabbccdd", 1000); err == nil {
		t.Fatalf("short salt should be rejected")
	}
	if _, err := VaultDeriveKey("p", "aabbccddaabbccdd", 0); err == nil {
		t.Fatalf("zero iterations should be rejected")
	}
}

// ---------- DEK 包裹 ----------

func TestVaultWrapUnwrapRoundtrip(t *testing.T) {
	dek, _ := VaultGenerateDEK()
	kek, _ := VaultDeriveKey("master-pass", mustSalt(t), 1000)
	wrapped, err := VaultWrapKey(kek, dek)
	if err != nil {
		t.Fatalf("wrap: %v", err)
	}
	got, err := VaultUnwrapKey(kek, wrapped)
	if err != nil {
		t.Fatalf("unwrap: %v", err)
	}
	if !bytes.Equal(dek, got) {
		t.Fatalf("roundtrip mismatch")
	}
	// 密文形态是 hex（落库列是字符串）
	for _, c := range wrapped {
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			t.Fatalf("wrapped key should be hex, got %q", wrapped)
		}
	}
}

func TestVaultUnwrapWrongPassword(t *testing.T) {
	dek, _ := VaultGenerateDEK()
	kek, _ := VaultDeriveKey("master-pass", mustSalt(t), 1000)
	wrapped, _ := VaultWrapKey(kek, dek)
	wrongKek, _ := VaultDeriveKey("master-pasS", mustSalt(t), 1000)
	if _, err := VaultUnwrapKey(wrongKek, wrapped); !errors.Is(err, ErrVaultWrongPassword) {
		t.Fatalf("wrong password must map to ErrVaultWrongPassword, got %v", err)
	}
	// 篡改密文同样折叠为同一错误（AEAD 防篡改）
	tampered := wrapped[:len(wrapped)-2] + "ff"
	if _, err := VaultUnwrapKey(kek, tampered); !errors.Is(err, ErrVaultWrongPassword) {
		t.Fatalf("tampered wrapped key must fail auth, got %v", err)
	}
}

// ---------- 凭据加密 ----------

func TestVaultEncryptDecryptStringRoundtrip(t *testing.T) {
	dek, _ := VaultGenerateDEK()
	secret := "p@ssw0rd-测试-🔐"
	enc, err := VaultEncryptString(dek, secret)
	if err != nil {
		t.Fatalf("encrypt: %v", err)
	}
	if bytes.Contains([]byte(enc), []byte(secret)) {
		t.Fatalf("plaintext leaked into ciphertext form")
	}
	got, err := VaultDecryptString(dek, enc)
	if err != nil || got != secret {
		t.Fatalf("roundtrip mismatch: %q vs %q (%v)", got, secret, err)
	}
	// 换一把 DEK 解不开
	other, _ := VaultGenerateDEK()
	if _, err := VaultDecryptString(other, enc); !errors.Is(err, ErrVaultWrongPassword) {
		t.Fatalf("wrong DEK must fail auth, got %v", err)
	}
}

func mustSalt(t *testing.T) string {
	t.Helper()
	s, err := VaultGenerateSalt()
	if err != nil {
		t.Fatalf("salt: %v", err)
	}
	return s
}
