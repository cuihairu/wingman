package middleware

import (
	"errors"
	"net/http"
	"os"
	"slices"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"gorm.io/gorm"
)

const jwtSecretMinLength = 32

var errJWTSecretNotConfigured = errors.New("WINGMAN_JWT_SECRET must be at least 32 characters")

// Claims JWT 声明
type Claims struct {
	UserID   uint   `json:"user_id"`
	Username string `json:"username"`
	Role     string `json:"role"`
	jwt.RegisteredClaims
}

func jwtSecret() ([]byte, error) {
	secret := os.Getenv("WINGMAN_JWT_SECRET")
	// Check minimum length
	if len(secret) < jwtSecretMinLength {
		return nil, errJWTSecretNotConfigured
	}
	// Trim and check again - reject secrets that are mostly whitespace
	trimmed := strings.TrimSpace(secret)
	if len(trimmed) < jwtSecretMinLength {
		return nil, errJWTSecretNotConfigured
	}
	// Reject secrets with leading/trailing whitespace (security issue)
	if trimmed != secret {
		return nil, errors.New("WINGMAN_JWT_SECRET must not have leading or trailing whitespace")
	}
	// Additional check: ensure secret contains non-whitespace characters
	hasNonWhitespace := false
	for _, c := range secret {
		if !strings.ContainsRune(" \t\n\r", c) {
			hasNonWhitespace = true
			break
		}
	}
	if !hasNonWhitespace {
		return nil, errors.New("WINGMAN_JWT_SECRET must contain non-whitespace characters")
	}
	return []byte(secret), nil
}

func ParseBearerToken(authHeader string) string {
	parts := strings.SplitN(authHeader, " ", 2)
	if len(parts) != 2 || parts[0] != "Bearer" {
		return ""
	}
	return parts[1]
}

func ValidateTokenString(tokenString string) (*Claims, error) {
	claims := &Claims{}
	token, err := jwt.ParseWithClaims(tokenString, claims, func(token *jwt.Token) (any, error) {
		if token.Method != jwt.SigningMethodHS256 {
			return nil, jwt.ErrTokenSignatureInvalid
		}
		return jwtSecret()
	})
	if err != nil || !token.Valid {
		return nil, jwt.ErrTokenInvalidClaims
	}
	return claims, nil
}

// AuthRequired JWT 认证中间件
func AuthRequired() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "Authorization header required"})
			c.Abort()
			return
		}

		tokenString := ParseBearerToken(authHeader)
		if tokenString == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "Invalid authorization format"})
			c.Abort()
			return
		}

		claims, err := ValidateTokenString(tokenString)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "Invalid token"})
			c.Abort()
			return
		}

		c.Set("user_id", claims.UserID)
		c.Set("username", claims.Username)
		c.Set("role", claims.Role)

		c.Next()
	}
}

// GenerateToken 生成 JWT token
func GenerateToken(userID uint, username, role string) (string, error) {
	now := time.Now()
	claims := Claims{
		UserID:   userID,
		Username: username,
		Role:     role,
		RegisteredClaims: jwt.RegisteredClaims{
			IssuedAt:  jwt.NewNumericDate(now),
			// Reduced from 24h to 15 minutes for better security
			// Short-lived tokens reduce the window for token theft
			// Consider implementing refresh tokens for better UX
			ExpiresAt: jwt.NewNumericDate(now.Add(15 * time.Minute)),
			Subject:   username,
		},
	}

	secret, err := jwtSecret()
	if err != nil {
		return "", err
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString(secret)
}

// GetCurrentUser 获取当前用户
func GetCurrentUser(c *gin.Context) (uint, string, string) {
	userID, _ := c.Get("user_id")
	username, _ := c.Get("username")
	role, _ := c.Get("role")
	if id, ok := userID.(uint); ok {
		if name, ok := username.(string); ok {
			if r, ok := role.(string); ok {
				return id, name, r
			}
		}
	}
	return 0, "", ""
}

// RoleRequired 角色检查中间件
func RoleRequired(roles ...string) gin.HandlerFunc {
	return func(c *gin.Context) {
		role, exists := c.Get("role")
		if !exists {
			c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "forbidden"})
			c.Abort()
			return
		}
		userRole, ok := role.(string)
		if !ok {
			c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "forbidden"})
			c.Abort()
			return
		}
		if !slices.Contains(roles, userRole) {
			c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "insufficient permissions"})
			c.Abort()
			return
		}
		c.Next()
	}
}

// PermissionRequired 细粒度权限检查中间件。
// admin 角色直接放行；否则从 DB 解析用户角色权限，要求命中任一指定权限或通配符。
// required 为空时退化为仅认证（已登录即可），便于渐进式接入。
func PermissionRequired(db *gorm.DB, required ...string) gin.HandlerFunc {
	return func(c *gin.Context) {
		if len(required) == 0 {
			c.Next()
			return
		}
		role, _ := c.Get("role")
		roleStr, _ := role.(string)

		// admin 超级用户放行
		if strings.EqualFold(strings.TrimSpace(roleStr), "admin") {
			c.Set("permissions", []string{"*"})
			c.Next()
			return
		}

		userIDVal, _ := c.Get("user_id")
		userID, _ := userIDVal.(uint)
		if userID == 0 {
			c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "forbidden"})
			c.Abort()
			return
		}

		codes, err := resolvePermissionCodes(c, db, userID)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to resolve permissions"})
			c.Abort()
			return
		}
		c.Set("permissions", codes)

		matched := false
		for _, req := range required {
			req = strings.TrimSpace(req)
			if req == "" {
				continue
			}
			for _, code := range codes {
				if code == "*" || strings.EqualFold(code, req) {
					matched = true
					break
				}
			}
			if matched {
				break
			}
		}
		if !matched {
			c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "insufficient permissions"})
			c.Abort()
			return
		}
		c.Next()
	}
}

// resolvePermissionCodes 从 DB 解析用户权限码（带请求级缓存，避免多个中间件重复查询）。
func resolvePermissionCodes(c *gin.Context, db *gorm.DB, userID uint) ([]string, error) {
	if cached, ok := c.Get("permissions"); ok {
		if codes, ok := cached.([]string); ok {
			return codes, nil
		}
	}
	// 此处避免与 rbac 包形成循环依赖，内联最小查询逻辑（使用 Find 避免 not-found 日志噪音）。
	var user struct {
		Role   string
		Active bool
	}
	if err := db.Table("users").Select("role, active").Where("id = ?", userID).Limit(1).Find(&user).Error; err != nil {
		return nil, err
	}
	if !user.Active {
		return nil, nil
	}
	if strings.EqualFold(strings.TrimSpace(user.Role), "admin") {
		return []string{"*"}, nil
	}
	var role struct{ ID uint }
	result := db.Table("roles").Select("id").Where("code = ?", user.Role).Limit(1).Find(&role)
	if result.Error != nil {
		return nil, result.Error
	}
	if role.ID == 0 {
		return nil, nil
	}
	var codes []string
	if err := db.Table("permissions").
		Select("permissions.code").
		Joins("JOIN role_permissions ON role_permissions.permission_id = permissions.id").
		Where("role_permissions.role_id = ?", role.ID).
		Pluck("permissions.code", &codes).Error; err != nil {
		return nil, err
	}
	return codes, nil
}
