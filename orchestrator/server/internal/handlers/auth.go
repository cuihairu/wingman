package handlers

import (
	"log"
	"net/http"
	"os"

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
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"error": "Invalid credentials",
		})
		return
	}

	if !security.VerifyPassword(user.Password, req.Password) {
		// Track failed attempts by IP and username combination
		key := clientIP + ":" + req.Username
		rl.Check(key)
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"error": "Invalid credentials",
		})
		return
	}

	// Record successful login to reset the rate limit counter
	key := clientIP + ":" + req.Username
	rl.RecordSuccess(key)

	token, err := middleware.GenerateToken(user.ID, user.Username, user.Role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"error": "Failed to generate token",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"token":   token,
		"user": gin.H{
			"id":       user.ID,
			"username": user.Username,
			"role":     user.Role,
		},
	})
}

// HandleLogout 登出
func (h *AuthHandler) HandleLogout(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"error": "Logged out",
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
		}
		h.db.Create(&admin)
	}
}
