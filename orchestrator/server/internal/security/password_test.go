package security

import (
	"strings"
	"testing"
)

func TestHashAndVerifyPassword(t *testing.T) {
	hash, err := HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatalf("hash: %v", err)
	}
	if hash == "" || hash == "Str0ng!pw" {
		t.Fatal("hash should be non-empty and differ from plaintext")
	}
	if !VerifyPassword(hash, "Str0ng!pw") {
		t.Error("correct password should verify")
	}
	if VerifyPassword(hash, "wrong") {
		t.Error("wrong password should not verify")
	}
	if VerifyPassword("not-a-bcrypt-hash", "Str0ng!pw") {
		t.Error("garbage hash should not verify")
	}
}

func TestHashPasswordRejectsTooLongPassword(t *testing.T) {
	// bcrypt 拒绝超过 72 字节的密码
	long := strings.Repeat("a", 73)
	if _, err := HashPassword(long); err == nil {
		t.Fatal("expected error for >72 byte password")
	}
}

func TestGetBcryptCost(t *testing.T) {
	if got := GetBcryptCost(); got != 12 {
		t.Errorf("expected cost 12, got %d", got)
	}
}

func TestValidatePasswordStrength(t *testing.T) {
	// 规则：长度 ≥8 且至少命中 大写/小写/数字/特殊字符 中的 3 类
	cases := []struct {
		name     string
		password string
		want     bool
	}{
		{"too short", "Ab1!x", false},
		{"only two classes lower+digit", "abcdefg123", false},
		{"only two classes upper+digit", "ABCDEFG123", false},
		{"only two classes upper+special", "ABCDEFG!XY", false},
		{"only two classes lower+special", "abcdefg!xy", false},
		{"upper+lower+digit", "Abcdefg123", true},
		{"upper+lower+special", "Abcdefg!xy", true},
		{"lower+digit+special", "abcdef1!xy", true},
		{"all four", "Abcdef1!xyz", true},
		{"unsupported special only counts as weak", `Abcdefg"xyz`, false},
		{"exactly eight with three classes", "Ab1!efgh", true},
	}
	for _, tc := range cases {
		if got := ValidatePasswordStrength(tc.password); got != tc.want {
			t.Errorf("%s: ValidatePasswordStrength(%q) = %v, want %v", tc.name, tc.password, got, tc.want)
		}
	}
}
