package handlers

import (
	"net/http"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// ProfileHandler exposes the minimum profile surface required by the dashboard.
type ProfileHandler struct {
	db *gorm.DB
}

func NewProfileHandler(db *gorm.DB) *ProfileHandler {
	return &ProfileHandler{db: db}
}

func (h *ProfileHandler) HandleGetProfile(c *gin.Context) {
	user, ok := h.currentUser(c)
	if !ok {
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"id":          user.ID,
		"username":    user.Username,
		"nickname":    user.Username,
		"displayName": user.Username,
		"roles":       []string{user.Role},
		"active":      true,
		"createdAt":   user.CreatedAt,
		"updatedAt":   user.UpdatedAt,
	})
}

func (h *ProfileHandler) HandleGetGames(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"games": []any{},
	})
}

func (h *ProfileHandler) HandleGetPermissions(c *gin.Context) {
	_, _, role := middleware.GetCurrentUser(c)
	isAdmin := strings.EqualFold(role, "admin")
	permissionIDs := []string{}
	if strings.TrimSpace(role) != "" {
		permissionIDs = append(permissionIDs, role)
	}
	if isAdmin && !contains(permissionIDs, "admin") {
		permissionIDs = append(permissionIDs, "admin")
	}

	c.JSON(http.StatusOK, gin.H{
		"permissions":   []any{},
		"admin":         isAdmin,
		"roles":         []string{role},
		"permissionIDs": permissionIDs,
	})
}

func (h *ProfileHandler) HandleUpdateProfile(c *gin.Context) {
	user, ok := h.currentUser(c)
	if !ok {
		return
	}

	var req struct {
		Nickname string `json:"nickname"`
		Email    string `json:"email"`
		Phone    string `json:"phone"`
		Avatar   string `json:"avatar"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"profile": gin.H{
			"id":          user.ID,
			"username":    user.Username,
			"nickname":    fallbackProfileValue(req.Nickname, user.Username),
			"displayName": fallbackProfileValue(req.Nickname, user.Username),
			"email":       req.Email,
			"phone":       req.Phone,
			"avatar":      req.Avatar,
			"roles":       []string{user.Role},
			"active":      true,
			"createdAt":   user.CreatedAt,
			"updatedAt":   user.UpdatedAt,
		},
	})
}

func (h *ProfileHandler) HandleUpdatePassword(c *gin.Context) {
	user, ok := h.currentUser(c)
	if !ok {
		return
	}

	var req struct {
		OldPassword string `json:"oldPassword" binding:"required"`
		NewPassword string `json:"newPassword" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "oldPassword and newPassword are required"})
		return
	}
	if !security.VerifyPassword(user.Password, req.OldPassword) {
		c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "invalid current password"})
		return
	}
	if !security.ValidatePasswordStrength(req.NewPassword) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "password does not meet strength requirements"})
		return
	}

	hash, err := security.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to hash password"})
		return
	}
	if err := h.db.Model(&user).Update("password", hash).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to update password"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"success": true})
}

func (h *ProfileHandler) currentUser(c *gin.Context) (models.User, bool) {
	userID, _, _ := middleware.GetCurrentUser(c)
	var user models.User
	if userID == 0 || h.db.First(&user, userID).Error != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "user not found"})
		return models.User{}, false
	}
	return user, true
}

func fallbackProfileValue(value, defaultValue string) string {
	if strings.TrimSpace(value) == "" {
		return defaultValue
	}
	return value
}

func contains(values []string, target string) bool {
	for _, value := range values {
		if strings.EqualFold(strings.TrimSpace(value), target) {
			return true
		}
	}
	return false
}
