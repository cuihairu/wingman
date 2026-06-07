package security

import (
	"golang.org/x/crypto/bcrypt"
	"log"
)

const bcryptCost = 12

func HashPassword(password string) (string, error) {
	// Using cost factor of 12 instead of bcrypt.DefaultCost (10)
	// This provides better protection against brute force attacks
	// while still being acceptable for modern hardware
	// Each increment doubles the time required to hash
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcryptCost)
	if err != nil {
		return "", err
	}
	return string(hash), nil
}

func VerifyPassword(hash, password string) bool {
	return bcrypt.CompareHashAndPassword([]byte(hash), []byte(password)) == nil
}

// GetBcryptCost returns the current bcrypt cost factor
func GetBcryptCost() int {
	return bcryptCost
}

// ValidatePasswordStrength checks if a password meets minimum security requirements
func ValidatePasswordStrength(password string) bool {
	if len(password) < 8 {
		log.Println("[Password] Password too short (minimum 8 characters)")
		return false
	}

	hasUpper := false
	hasLower := false
	hasDigit := false
	hasSpecial := false

	for _, c := range password {
		switch {
		case c >= 'A' && c <= 'Z':
			hasUpper = true
		case c >= 'a' && c <= 'z':
			hasLower = true
		case c >= '0' && c <= '9':
			hasDigit = true
		case c == '!' || c == '@' || c == '#' || c == '$' || c == '%' || c == '&' || c == '*' || c == '?' || c == '.':
			hasSpecial = true
		}
	}

	strengthScore := 0
	if hasUpper {
		strengthScore++
	}
	if hasLower {
		strengthScore++
	}
	if hasDigit {
		strengthScore++
	}
	if hasSpecial {
		strengthScore++
	}

	if strengthScore < 3 {
		log.Println("[Password] Password too weak (requires uppercase, lowercase, digit, or special character)")
		return false
	}

	return true
}
