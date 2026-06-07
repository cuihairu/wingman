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
