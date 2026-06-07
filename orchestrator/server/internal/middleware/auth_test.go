package middleware

import "testing"

func TestGenerateTokenRejectsMissingSecret(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "")

	if _, err := GenerateToken(1, "admin", "admin"); err == nil {
		t.Fatal("GenerateToken accepted an empty WINGMAN_JWT_SECRET")
	}
}

func TestValidateTokenRejectsMissingSecret(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")

	token, err := GenerateToken(1, "admin", "admin")
	if err != nil {
		t.Fatalf("GenerateToken failed: %v", err)
	}

	t.Setenv("WINGMAN_JWT_SECRET", "")
	if _, err := ValidateTokenString(token); err == nil {
		t.Fatal("ValidateTokenString accepted a token with an empty WINGMAN_JWT_SECRET")
	}
}

func TestGenerateAndValidateToken(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")

	token, err := GenerateToken(7, "operator", "admin")
	if err != nil {
		t.Fatalf("GenerateToken failed: %v", err)
	}

	claims, err := ValidateTokenString(token)
	if err != nil {
		t.Fatalf("ValidateTokenString failed: %v", err)
	}
	if claims.UserID != 7 || claims.Username != "operator" || claims.Role != "admin" {
		t.Fatalf("unexpected claims: %+v", claims)
	}
}

func TestGenerateTokenRejectsShortSecret(t *testing.T) {
	// 31 characters — just below minimum
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcde")

	if _, err := GenerateToken(1, "admin", "admin"); err == nil {
		t.Fatal("GenerateToken accepted a 31-character WINGMAN_JWT_SECRET")
	}
}

func TestGenerateTokenAcceptsExactMinLength(t *testing.T) {
	// Exactly 32 characters — the minimum
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")

	if _, err := GenerateToken(1, "admin", "admin"); err != nil {
		t.Fatalf("GenerateToken rejected a valid 32-character secret: %v", err)
	}
}

func TestGenerateTokenRejectsWhitespaceOnlySecret(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "                                ") // 32 spaces

	if _, err := GenerateToken(1, "admin", "admin"); err == nil {
		t.Fatal("GenerateToken accepted a whitespace-only WINGMAN_JWT_SECRET")
	}
}

func TestValidateTokenRejectsTamperedToken(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")

	token, err := GenerateToken(1, "admin", "admin")
	if err != nil {
		t.Fatalf("GenerateToken failed: %v", err)
	}

	// Tamper with the token
	tampered := token + "x"
	if _, err := ValidateTokenString(tampered); err == nil {
		t.Fatal("ValidateTokenString accepted a tampered token")
	}
}

func TestValidateTokenRejectsWrongSecret(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "alpha-secret-key-that-is-32-chars!!")

	token, err := GenerateToken(1, "admin", "admin")
	if err != nil {
		t.Fatalf("GenerateToken failed: %v", err)
	}

	// Switch to a different valid secret
	t.Setenv("WINGMAN_JWT_SECRET", "bravo-secret-key-that-is-32-chars!!")
	if _, err := ValidateTokenString(token); err == nil {
		t.Fatal("ValidateTokenString accepted a token signed with a different secret")
	}
}
