package handlers

import (
	"errors"
	"net/http"
	"strings"
	"unicode/utf8"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

const (
	minUsernameLen = 3
	maxUsernameLen = 32
)

// UserHandler 用户管理（admin）
type UserHandler struct {
	db *gorm.DB
}

func NewUserHandler(db *gorm.DB) *UserHandler {
	return &UserHandler{db: db}
}

type createUserReq struct {
	Username string `json:"username" binding:"required"`
	Password string `json:"password" binding:"required"`
	Role     string `json:"role"`
	Active   *bool  `json:"active"`
}

type updateUserReq struct {
	Role   string `json:"role"`
	Active *bool  `json:"active"`
}

type resetPasswordReq struct {
	NewPassword string `json:"newPassword" binding:"required"`
}

type listUsersResponse struct {
	Items []models.SafeUser `json:"items"`
	Total int64             `json:"total"`
}

// HandleList 用户列表（分页）
// @Summary      用户列表（分页/过滤）
// @Tags         admin-users
// @Produce      json
// @Security     BearerAuth
// @Param        page      query  int     false  "页码"        default(1)
// @Param        size      query  int     false  "每页条数"    default(20)
// @Param        role      query  string  false  "角色过滤"
// @Param        keyword   query  string  false  "关键字"
// @Success      200  {object}  map[string]interface{}
// @Router       /admin/users [get]
func (h *UserHandler) HandleList(c *gin.Context) {
	page := parsePositiveInt(c.Query("page"), 1)
	size := parsePositiveInt(c.Query("size"), 20)
	if size > 200 {
		size = 200
	}

	query := h.db.Model(&models.User{})
	if role := strings.TrimSpace(c.Query("role")); role != "" {
		query = query.Where("role = ?", role)
	}
	if kw := strings.TrimSpace(c.Query("keyword")); kw != "" {
		query = query.Where("username LIKE ?", "%"+kw+"%")
	}

	var total int64
	query.Count(&total)

	var users []models.User
	if err := query.Order("id asc").Offset((page - 1) * size).Limit(size).Find(&users).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load users"})
		return
	}

	items := make([]models.SafeUser, 0, len(users))
	for _, u := range users {
		items = append(items, u.ToSafe())
	}
	c.JSON(http.StatusOK, listUsersResponse{Items: items, Total: total})
}

// HandleGet 单个用户
func (h *UserHandler) HandleGet(c *gin.Context) {
	user, ok := h.findByIDParam(c)
	if !ok {
		return
	}
	c.JSON(http.StatusOK, user.ToSafe())
}

// HandleCreate 创建用户
// @Summary      创建用户
// @Tags         admin-users
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body      object  true  "用户创建"  example({"username":"bob","password":"...","role":"viewer"})
// @Success      201  {object}  map[string]interface{}
// @Router       /admin/users [post]
func (h *UserHandler) HandleCreate(c *gin.Context) {
	var req createUserReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "username and password are required"})
		return
	}
	req.Username = strings.TrimSpace(req.Username)
	if !validUsername(req.Username) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "username must be 3-32 chars of letters/digits/_/-"})
		return
	}
	if !security.ValidatePasswordStrength(req.Password) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "password does not meet strength requirements"})
		return
	}
	role := strings.TrimSpace(req.Role)
	if role == "" {
		role = "viewer"
	}
	ok, err := rbac.EnsureRoleExists(h.db, role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to validate role"})
		return
	}
	if !ok {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "unknown role: " + role})
		return
	}

	hash, err := security.HashPassword(req.Password)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to hash password"})
		return
	}
	active := true
	if req.Active != nil {
		active = *req.Active
	}
	user := models.User{Username: req.Username, Password: hash, Role: role, Active: active}
	if err := h.db.Create(&user).Error; err != nil {
		if isUniqueConstraint(err) {
			c.JSON(http.StatusConflict, gin.H{"success": false, "error": "username already exists"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to create user"})
		return
	}

	actor, _ := actorName(c)
	WriteAuditLog(h.db, actor, "user.create", "user:"+user.Username, map[string]any{"id": user.ID, "role": user.Role})
	c.JSON(http.StatusCreated, gin.H{"success": true, "user": user.ToSafe()})
}

// HandleUpdate 更新用户（角色/启用状态；不改密码）
func (h *UserHandler) HandleUpdate(c *gin.Context) {
	user, ok := h.findByIDParam(c)
	if !ok {
		return
	}
	var req updateUserReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request"})
		return
	}

	updates := map[string]any{}
	if role := strings.TrimSpace(req.Role); role != "" {
		roleOK, err := rbac.EnsureRoleExists(h.db, role)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to validate role"})
			return
		}
		if !roleOK {
			c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "unknown role: " + role})
			return
		}
		updates["role"] = role
	}
	if req.Active != nil {
		updates["active"] = *req.Active
	}

	if len(updates) == 0 {
		c.JSON(http.StatusOK, gin.H{"success": true, "user": user.ToSafe()})
		return
	}
	if err := h.db.Model(&user).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to update user"})
		return
	}
	h.db.First(&user, user.ID)

	actor, selfID := actorName(c)
	meta := map[string]any{"id": user.ID, "changes": updates}
	if selfID == user.ID {
		meta["self"] = true
	}
	WriteAuditLog(h.db, actor, "user.update", "user:"+user.Username, meta)
	c.JSON(http.StatusOK, gin.H{"success": true, "user": user.ToSafe()})
}

// HandleResetPassword 重置用户密码
func (h *UserHandler) HandleResetPassword(c *gin.Context) {
	user, ok := h.findByIDParam(c)
	if !ok {
		return
	}
	var req resetPasswordReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "newPassword is required"})
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
	actor, _ := actorName(c)
	WriteAuditLog(h.db, actor, "user.reset_password", "user:"+user.Username, nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleDelete 删除用户（禁止删除自身与内置 admin）
func (h *UserHandler) HandleDelete(c *gin.Context) {
	user, ok := h.findByIDParam(c)
	if !ok {
		return
	}
	actor, selfID := actorName(c)
	if user.ID == selfID {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "cannot delete yourself"})
		return
	}
	if strings.EqualFold(user.Username, "admin") {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "cannot delete builtin admin account"})
		return
	}
	if err := h.db.Delete(&user).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to delete user"})
		return
	}
	WriteAuditLog(h.db, actor, "user.delete", "user:"+user.Username, map[string]any{"id": user.ID})
	c.JSON(http.StatusOK, gin.H{"success": true})
}

func (h *UserHandler) findByIDParam(c *gin.Context) (models.User, bool) {
	id := parsePositiveInt(c.Param("id"), 0)
	if id == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid user id"})
		return models.User{}, false
	}
	var user models.User
	if err := h.db.First(&user, id).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "user not found"})
			return models.User{}, false
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load user"})
		return models.User{}, false
	}
	return user, true
}

func validUsername(name string) bool {
	n := utf8.RuneCountInString(name)
	if n < minUsernameLen || n > maxUsernameLen {
		return false
	}
	for _, r := range name {
		switch {
		case r >= 'a' && r <= 'z', r >= 'A' && r <= 'Z', r >= '0' && r <= '9', r == '_', r == '-':
			continue
		default:
			return false
		}
	}
	return true
}

func actorName(c *gin.Context) (string, uint) {
	_, username, _ := middleware.GetCurrentUser(c)
	userID, _ := c.Get("user_id")
	id, _ := userID.(uint)
	return username, id
}

func isUniqueConstraint(err error) bool {
	if err == nil {
		return false
	}
	msg := err.Error()
	return strings.Contains(msg, "UNIQUE constraint") || strings.Contains(msg, "Duplicate entry")
}
