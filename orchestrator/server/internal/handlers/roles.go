package handlers

import (
	"errors"
	"net/http"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// RoleHandler 角色与权限目录管理（admin）
type RoleHandler struct {
	db *gorm.DB
}

func NewRoleHandler(db *gorm.DB) *RoleHandler {
	return &RoleHandler{db: db}
}

type roleDTO struct {
	ID          uint                 `json:"id"`
	Code        string               `json:"code"`
	Name        string               `json:"name"`
	Description string               `json:"description"`
	Builtin     bool                 `json:"builtin"`
	Permissions []models.Permission  `json:"permissions"`
	CreatedAt   string               `json:"createdAt"`
}

type createRoleReq struct {
	Code        string   `json:"code" binding:"required"`
	Name        string   `json:"name"`
	Description string   `json:"description"`
	Permissions []string `json:"permissions"`
}

type updateRoleReq struct {
	Name        string   `json:"name"`
	Description string   `json:"description"`
	Permissions []string `json:"permissions"`
}

func toRoleDTO(r models.Role) roleDTO {
	dto := roleDTO{
		ID:          r.ID,
		Code:        r.Code,
		Name:        r.Name,
		Description: r.Description,
		Builtin:     r.Builtin,
		Permissions: r.Permissions,
	}
	if !r.CreatedAt.IsZero() {
		dto.CreatedAt = r.CreatedAt.Format("2006-01-02 15:04:05")
	}
	if dto.Permissions == nil {
		dto.Permissions = []models.Permission{}
	}
	return dto
}

// HandleListRoles 角色列表（含权限）
func (h *RoleHandler) HandleListRoles(c *gin.Context) {
	var roles []models.Role
	if err := h.db.Preload("Permissions").Order("builtin desc, id asc").Find(&roles).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load roles"})
		return
	}
	items := make([]roleDTO, 0, len(roles))
	for _, r := range roles {
		items = append(items, toRoleDTO(r))
	}
	c.JSON(http.StatusOK, gin.H{"items": items, "total": len(items)})
}

// HandleGetRole 单个角色
func (h *RoleHandler) HandleGetRole(c *gin.Context) {
	role, ok := h.findByCodeParam(c)
	if !ok {
		return
	}
	c.JSON(http.StatusOK, toRoleDTO(role))
}

// HandleListPermissions 权限目录
func (h *RoleHandler) HandleListPermissions(c *gin.Context) {
	query := h.db.Model(&models.Permission{})
	if cat := strings.TrimSpace(c.Query("category")); cat != "" {
		query = query.Where("category = ?", cat)
	}
	var perms []models.Permission
	if err := query.Order("category asc, code asc").Find(&perms).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load permissions"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"items": perms, "total": len(perms)})
}

// HandleCreateRole 创建自定义角色
func (h *RoleHandler) HandleCreateRole(c *gin.Context) {
	var req createRoleReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "code is required"})
		return
	}
	req.Code = strings.TrimSpace(req.Code)
	if !validRoleCode(req.Code) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "code must be 2-32 chars of letters/digits/_/-"})
		return
	}
	if rbac.IsAdmin(req.Code) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "reserved role code"})
		return
	}

	role := models.Role{Code: req.Code, Name: strings.TrimSpace(req.Name), Description: req.Description}
	if err := h.db.Create(&role).Error; err != nil {
		if isUniqueConstraint(err) {
			c.JSON(http.StatusConflict, gin.H{"success": false, "error": "role code already exists"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to create role"})
		return
	}
	if len(req.Permissions) > 0 {
		if err := h.assignPermissions(c, &role, req.Permissions); err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
			return
		}
	}

	actor, _ := actorName(c)
	WriteAuditLog(h.db, actor, "role.create", "role:"+role.Code, map[string]any{"permissions": req.Permissions})
	h.db.Preload("Permissions").First(&role, role.ID)
	c.JSON(http.StatusCreated, gin.H{"success": true, "role": toRoleDTO(role)})
}

// HandleUpdateRole 更新角色（内置角色仅允许改权限，不改 code/name）
func (h *RoleHandler) HandleUpdateRole(c *gin.Context) {
	role, ok := h.findByCodeParam(c)
	if !ok {
		return
	}

	var req updateRoleReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request"})
		return
	}

	if !role.Builtin {
		if name := strings.TrimSpace(req.Name); name != "" {
			role.Name = name
		}
		if req.Description != "" {
			role.Description = req.Description
		}
		h.db.Model(&role).Updates(map[string]any{"name": role.Name, "description": role.Description})
	}

	// 权限可更新（含内置角色，便于微调；admin 角色保持通配不变）
	if role.Code != rbac.AdminRole && req.Permissions != nil {
		if err := h.assignPermissions(c, &role, req.Permissions); err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
			return
		}
	}

	actor, _ := actorName(c)
	WriteAuditLog(h.db, actor, "role.update", "role:"+role.Code, map[string]any{"permissions": req.Permissions})
	h.db.Preload("Permissions").First(&role, role.ID)
	c.JSON(http.StatusOK, gin.H{"success": true, "role": toRoleDTO(role)})
}

// HandleDeleteRole 删除自定义角色（内置角色禁止删除）
func (h *RoleHandler) HandleDeleteRole(c *gin.Context) {
	role, ok := h.findByCodeParam(c)
	if !ok {
		return
	}
	if role.Builtin {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "cannot delete builtin role"})
		return
	}
	// 防止删除仍被用户引用的角色
	var count int64
	h.db.Model(&models.User{}).Where("role = ?", role.Code).Count(&count)
	if count > 0 {
		c.JSON(http.StatusConflict, gin.H{"success": false, "error": "role is still assigned to users"})
		return
	}
	if err := h.db.Select("Permissions").Delete(&role).Error; err != nil { // 清理多对多关联
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to delete role"})
		return
	}
	actor, _ := actorName(c)
	WriteAuditLog(h.db, actor, "role.delete", "role:"+role.Code, nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

func (h *RoleHandler) findByCodeParam(c *gin.Context) (models.Role, bool) {
	code := strings.TrimSpace(c.Param("code"))
	if code == "" {
		// 兼容 :id 形式
		code = strings.TrimSpace(c.Param("id"))
	}
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "role code is required"})
		return models.Role{}, false
	}
	var role models.Role
	err := h.db.Preload("Permissions").Where("code = ? OR id = ?", code, code).First(&role).Error
	if err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "role not found"})
			return models.Role{}, false
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load role"})
		return models.Role{}, false
	}
	return role, true
}

// assignPermissions 校验并替换角色权限
func (h *RoleHandler) assignPermissions(c *gin.Context, role *models.Role, codes []string) error {
	codes = normalizeCodes(codes)
	if len(codes) == 0 {
		return h.db.Model(role).Association("Permissions").Clear()
	}
	var perms []models.Permission
	if err := h.db.Where("code IN ?", codes).Find(&perms).Error; err != nil {
		return errors.New("failed to load permissions")
	}
	if len(perms) != len(codes) {
		return errors.New("some permission codes are unknown")
	}
	return h.db.Model(role).Association("Permissions").Replace(&perms)
}

func normalizeCodes(codes []string) []string {
	out := make([]string, 0, len(codes))
	seen := map[string]bool{}
	for _, c := range codes {
		c = strings.TrimSpace(c)
		if c == "" || seen[c] {
			continue
		}
		seen[c] = true
		out = append(out, c)
	}
	return out
}

func validRoleCode(code string) bool {
	n := len(code)
	if n < 2 || n > 32 {
		return false
	}
	for _, r := range code {
		switch {
		case r >= 'a' && r <= 'z', r >= 'A' && r <= 'Z', r >= '0' && r <= '9', r == '_', r == '-':
			continue
		default:
			return false
		}
	}
	return true
}
