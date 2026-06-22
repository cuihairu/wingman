package handlers

import (
	"net/http"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
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

// @Summary      获取当前用户资料
// @Tags         profile
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /v1/profile [get]
func (h *ProfileHandler) HandleGetProfile(c *gin.Context) {
	user, ok := h.currentUser(c)
	if !ok {
		return
	}

	c.JSON(http.StatusOK, profilePayload(user))
}

func (h *ProfileHandler) HandleGetGames(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"games": []any{},
	})
}

// @Summary      获取当前用户权限码（resource:action 展开）
// @Tags         profile
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /v1/profile/permissions [get]
func (h *ProfileHandler) HandleGetPermissions(c *gin.Context) {
	user, ok := h.currentUser(c)
	if !ok {
		return
	}
	role := user.Role
	isAdmin := rbac.IsAdmin(role)

	codes, err := rbac.UserPermissionCodes(h.db, user.ID)
	if err != nil || codes == nil {
		codes = []string{}
	}

	// 权限码作为前端 access token（resource:action 形式，与 access.ts 对齐）
	permissionIDs := codes
	if isAdmin && !contains(permissionIDs, rbac.Wildcard) {
		permissionIDs = append([]string{rbac.Wildcard}, permissionIDs...)
	}

	permissions := make([]gin.H, 0, len(codes))
	for _, code := range codes {
		perm := codeToPermission(code)
		permissions = append(permissions, perm)
	}

	c.JSON(http.StatusOK, gin.H{
		"permissions":   permissions,
		"admin":         isAdmin,
		"roles":         []string{role},
		"permissionIDs": permissionIDs,
	})
}

// codeToPermission 将权限码展开为前端期望的 {resource, actions[]} 结构
func codeToPermission(code string) gin.H {
	resource, action := code, "*"
	if idx := strings.Index(code, ":"); idx > 0 {
		resource = code[:idx]
		action = code[idx+1:]
	}
	return gin.H{
		"code":     code,
		"resource": resource,
		"actions":  []string{action},
	}
}

func (h *ProfileHandler) HandleUpdateProfile(c *gin.Context) {
	user, ok := h.currentUser(c)
	if !ok {
		return
	}

	var req struct {
		Nickname *string `json:"nickname"`
		Email    *string `json:"email"`
		Phone    *string `json:"phone"`
		Avatar   *string `json:"avatar"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request"})
		return
	}

	updates := map[string]any{}
	if req.Nickname != nil {
		updates["nickname"] = strings.TrimSpace(*req.Nickname)
	}
	if req.Email != nil {
		updates["email"] = strings.TrimSpace(*req.Email)
	}
	if req.Phone != nil {
		updates["phone"] = strings.TrimSpace(*req.Phone)
	}
	if req.Avatar != nil {
		updates["avatar"] = strings.TrimSpace(*req.Avatar)
	}
	if len(updates) > 0 {
		if err := h.db.Model(&user).Updates(updates).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to update profile"})
			return
		}
		h.db.First(&user, user.ID)
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"profile": profilePayload(user),
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

func profilePayload(user models.User) gin.H {
	displayName := fallbackProfileValue(user.Nickname, user.Username)
	return gin.H{
		"id":          user.ID,
		"username":    user.Username,
		"nickname":    displayName,
		"displayName": displayName,
		"email":       user.Email,
		"phone":       user.Phone,
		"avatar":      user.Avatar,
		"roles":       []string{user.Role},
		"active":      user.Active,
		"createdAt":   user.CreatedAt,
		"updatedAt":   user.UpdatedAt,
		"lastLoginAt": user.LastLoginAt,
	}
}

func contains(values []string, target string) bool {
	for _, value := range values {
		if strings.EqualFold(strings.TrimSpace(value), target) {
			return true
		}
	}
	return false
}
