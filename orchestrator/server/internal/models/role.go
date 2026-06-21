package models

import (
	"gorm.io/gorm"
)

// Permission 权限定义（资源:动作 形式，如 "users:manage"、"scripts:run"）
type Permission struct {
	gorm.Model
	Code        string `gorm:"uniqueIndex;not null" json:"code"`
	Name        string `json:"name"`
	Description string `json:"description"`
	Category    string `gorm:"index" json:"category"`
	Builtin     bool   `gorm:"default:false" json:"builtin"`
}

// Role 角色
type Role struct {
	gorm.Model
	Code        string       `gorm:"uniqueIndex;not null" json:"code"`
	Name        string       `json:"name"`
	Description string       `json:"description"`
	Builtin     bool         `gorm:"default:false" json:"builtin"`
	Permissions []Permission `gorm:"many2many:role_permissions;" json:"permissions"`
}

// PermissionCodes 返回角色的权限码集合（用于鉴权与下发前端）
func (r *Role) PermissionCodes() []string {
	codes := make([]string, 0, len(r.Permissions))
	for _, p := range r.Permissions {
		if p.Code != "" {
			codes = append(codes, p.Code)
		}
	}
	return codes
}

// LoadRolePermissions 填充角色的权限列表（避免调用方重复写 Preload）
func LoadRolePermissions(db *gorm.DB, role *Role) error {
	return db.Model(role).Association("Permissions").Find(&role.Permissions)
}
