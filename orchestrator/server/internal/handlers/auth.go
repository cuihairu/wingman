package handlers

import (
	"log"
	"net/http"
	"os"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// AuthHandler 认证处理器
type AuthHandler struct {
	db *gorm.DB
}

// NewAuthHandler 创建认证处理器
func NewAuthHandler(db *gorm.DB) *AuthHandler {
	return &AuthHandler{db: db}
}

// HandleLogin 登录
func (h *AuthHandler) HandleLogin(c *gin.Context) {
	// Get rate limiter and client IP for tracking failed attempts
	rl := middleware.GetRateLimiter()
	clientIP := c.ClientIP()

	var req struct {
		Username string `json:"username" binding:"required"`
		Password string `json:"password" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "Username and password required"})
		return
	}

	var user models.User
	if err := h.db.Where("username = ?", req.Username).First(&user).Error; err != nil {
		// Rate limiting based on IP, but also use username as part of the key for better tracking
		// This prevents an attacker from trying multiple usernames from the same IP
		key := clientIP + ":" + req.Username
		rl.Check(key) // Increment the counter (we don't care about return value here as we're already using middleware)
		WriteAuditLog(h.db, req.Username, "login_fail", "auth.login", loginAuditMeta(clientIP, c.GetHeader("User-Agent"), "user_not_found"))
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"error":   "Invalid credentials",
		})
		return
	}

	if !security.VerifyPassword(user.Password, req.Password) {
		// Track failed attempts by IP and username combination
		key := clientIP + ":" + req.Username
		rl.Check(key)
		WriteAuditLog(h.db, req.Username, "login_fail", "auth.login", loginAuditMeta(clientIP, c.GetHeader("User-Agent"), "invalid_password"))
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"error":   "Invalid credentials",
		})
		return
	}
	if !user.Active {
		WriteAuditLog(h.db, user.Username, "login_fail", "auth.login", loginAuditMeta(clientIP, c.GetHeader("User-Agent"), "inactive_user"))
		c.JSON(http.StatusForbidden, gin.H{
			"success": false,
			"error":   "User is disabled",
		})
		return
	}

	// Record successful login to reset the rate limit counter
	key := clientIP + ":" + req.Username
	rl.RecordSuccess(key)
	WriteAuditLog(h.db, user.Username, "login", "auth.login", loginAuditMeta(clientIP, c.GetHeader("User-Agent"), "success"))

	token, err := middleware.GenerateToken(user.ID, user.Username, user.Role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"error":   "Failed to generate token",
		})
		return
	}
	now := time.Now()
	if err := h.db.Model(&user).Update("last_login_at", now).Error; err != nil {
		log.Printf("Failed to update last login time for user %s: %v", user.Username, err)
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"token":   token,
		"user": gin.H{
			"id":       user.ID,
			"username": user.Username,
			"role":     user.Role,
			"active":   user.Active,
		},
	})
}

func loginAuditMeta(clientIP, userAgent, result string) map[string]any {
	return map[string]any{
		"ip":         clientIP,
		"ua":         userAgent,
		"user_agent": userAgent,
		"result":     result,
	}
}

// HandleLogout 登出
func (h *AuthHandler) HandleLogout(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"error":   "Logged out",
	})
}

// InitAdmin 初始化管理员账户
func (h *AuthHandler) InitAdmin() {
	var count int64
	h.db.Model(&models.User{}).Count(&count)
	if count == 0 {
		password := os.Getenv("WINGMAN_ADMIN_PASSWORD")
		if password == "" {
			log.Println("No users exist. Set WINGMAN_ADMIN_PASSWORD to bootstrap the initial admin account.")
			return
		}
		hash, err := security.HashPassword(password)
		if err != nil {
			log.Printf("Failed to hash bootstrap admin password: %v", err)
			return
		}
		admin := models.User{
			Username: "admin",
			Password: hash,
			Role:     "admin",
			Active:   true,
		}
		h.db.Create(&admin)
	}
}
