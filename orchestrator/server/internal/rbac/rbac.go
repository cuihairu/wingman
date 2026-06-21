// Package rbac 提供基于角色-权限的访问控制解析与内置种子数据。
//
// 设计要点：
//   - User.Role 为字符串角色码（与现有 JWT claims / RoleRequired 兼容），对应 Role.Code。
//   - admin 角色作为超级用户，PermissionRequired 与权限解析对其直接放行。
//   - 权限码采用 "resource:action" 形式（如 users:manage），与 dashboard access.ts 对齐；
//     "*" 表示通配全部权限。
package rbac

import (
	"errors"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

// AdminRole 超级用户角色码
const AdminRole = "admin"

// Wildcard 通配权限码
const Wildcard = "*"

// builtinPermissions 内置权限目录（resource:action）
var builtinPermissions = []models.Permission{
	{Code: "users:manage", Name: "用户管理", Description: "创建/编辑/删除用户及重置密码", Category: "admin", Builtin: true},
	{Code: "roles:manage", Name: "角色管理", Description: "创建/编辑角色并分配权限", Category: "admin", Builtin: true},
	{Code: "agents:view", Name: "查看 Agent", Description: "查看 agent 列表与状态", Category: "agent", Builtin: true},
	{Code: "agents:manage", Name: "管理 Agent", Description: "关闭 agent 等控制操作", Category: "agent", Builtin: true},
	{Code: "scripts:view", Name: "查看脚本", Description: "查看脚本列表与内容", Category: "script", Builtin: true},
	{Code: "scripts:edit", Name: "编辑脚本", Description: "创建/保存/删除脚本", Category: "script", Builtin: true},
	{Code: "scripts:run", Name: "运行脚本", Description: "启动/停止脚本", Category: "script", Builtin: true},
	{Code: "workflows:view", Name: "查看工作流", Description: "查看工作流列表与详情", Category: "workflow", Builtin: true},
	{Code: "workflows:run", Name: "运行工作流", Description: "提交/取消工作流", Category: "workflow", Builtin: true},
	{Code: "monitor:view", Name: "监控查看", Description: "查看实时监控与截图", Category: "monitor", Builtin: true},
	{Code: "audit:view", Name: "审计查看", Description: "查看登录/操作审计日志", Category: "admin", Builtin: true},
	{Code: "settings:view", Name: "查看设置", Description: "读取系统设置", Category: "settings", Builtin: true},
	{Code: "settings:edit", Name: "编辑设置", Description: "更新系统设置", Category: "settings", Builtin: true},
	{Code: "debugger:use", Name: "调试器", Description: "使用 Lua 调试器", Category: "debugger", Builtin: true},
}

// builtinRoles 内置角色及其权限码
var builtinRoles = []struct {
	Role  models.Role
	Codes []string
}{
	{
		Role:  models.Role{Code: AdminRole, Name: "管理员", Description: "超级用户，拥有全部权限", Builtin: true},
		Codes: allPermissionCodes(),
	},
	{
		Role:  models.Role{Code: "operator", Name: "操作员", Description: "可运行脚本/工作流并监控", Builtin: true},
		Codes: []string{"agents:view", "agents:manage", "scripts:view", "scripts:edit", "scripts:run", "workflows:view", "workflows:run", "monitor:view", "audit:view", "settings:view"},
	},
	{
		Role:  models.Role{Code: "viewer", Name: "只读用户", Description: "仅查看权限", Builtin: true},
		Codes: []string{"agents:view", "scripts:view", "workflows:view", "monitor:view", "audit:view", "settings:view"},
	},
}

func allPermissionCodes() []string {
	codes := make([]string, 0, len(builtinPermissions)+1)
	codes = append(codes, Wildcard)
	for _, p := range builtinPermissions {
		codes = append(codes, p.Code)
	}
	return codes
}

// Seed 内置权限与角色。幂等：已存在则跳过；权限目录缺失时补齐。
func Seed(db *gorm.DB) error {
	if db == nil {
		return errors.New("rbac: nil db")
	}

	// 1. 权限目录（使用 Find 而非 First，避免 ErrRecordNotFound 日志噪音）
	for _, p := range builtinPermissions {
		var existing models.Permission
		db.Where("code = ?", p.Code).Limit(1).Find(&existing)
		if existing.ID != 0 {
			if existing.Name == "" || existing.Category == "" {
				db.Model(&existing).Updates(map[string]any{"name": p.Name, "description": p.Description, "category": p.Category, "builtin": p.Builtin})
			}
			continue
		}
		perm := p
		perm.ID = 0
		if err := db.Create(&perm).Error; err != nil {
			return err
		}
	}

	// 2. 角色 + 权限分配
	for _, br := range builtinRoles {
		var role models.Role
		db.Where("code = ?", br.Role.Code).Limit(1).Find(&role)
		if role.ID == 0 {
			role = br.Role
			role.ID = 0
			if err := db.Create(&role).Error; err != nil {
				return err
			}
		} else if role.Name == "" {
			db.Model(&role).Updates(map[string]any{"name": br.Role.Name, "description": br.Role.Description, "builtin": true})
		}

		// 内置角色每次启动同步权限（保证目录完整）
		if role.Builtin {
			var perms []models.Permission
			if err := db.Where("code IN ?", br.Codes).Find(&perms).Error; err != nil {
				return err
			}
			if err := db.Model(&role).Association("Permissions").Replace(&perms); err != nil {
				return err
			}
		}
	}

	return nil
}

// IsAdmin 判断角色码是否为超级用户
func IsAdmin(roleCode string) bool {
	return strings.EqualFold(strings.TrimSpace(roleCode), AdminRole)
}

// RolePermissionCodes 解析角色拥有的权限码。admin 直接返回通配；角色不存在返回空。
func RolePermissionCodes(db *gorm.DB, roleCode string) ([]string, error) {
	if IsAdmin(roleCode) {
		return []string{Wildcard}, nil
	}
	var role models.Role
	result := db.Preload("Permissions").Where("code = ?", roleCode).Limit(1).Find(&role)
	if result.Error != nil {
		return nil, result.Error
	}
	if role.ID == 0 {
		return nil, nil
	}
	return role.PermissionCodes(), nil
}

// UserPermissionCodes 解析用户权限码（经其角色）。用户不存在或未激活返回空。
func UserPermissionCodes(db *gorm.DB, userID uint) ([]string, error) {
	var user models.User
	result := db.Select("id, role, active").Where("id = ?", userID).Limit(1).Find(&user)
	if result.Error != nil {
		return nil, result.Error
	}
	if user.ID == 0 || !user.Active {
		return nil, nil
	}
	return RolePermissionCodes(db, user.Role)
}

// HasPermission 判断用户是否拥有指定权限。admin 放行；通配 "*" 放行。
func HasPermission(db *gorm.DB, userID uint, required string) (bool, error) {
	codes, err := UserPermissionCodes(db, userID)
	if err != nil {
		return false, err
	}
	required = strings.TrimSpace(required)
	for _, c := range codes {
		if c == Wildcard || strings.EqualFold(c, required) {
			return true, nil
		}
	}
	return false, nil
}

// EnsureRoleExists 校验角色码在角色表中存在（用于创建/更新用户时校验 Role 字段）
func EnsureRoleExists(db *gorm.DB, roleCode string) (bool, error) {
	if IsAdmin(roleCode) {
		return true, nil
	}
	var count int64
	if err := db.Model(&models.Role{}).Where("code = ?", roleCode).Count(&count).Error; err != nil {
		return false, err
	}
	return count > 0, nil
}
