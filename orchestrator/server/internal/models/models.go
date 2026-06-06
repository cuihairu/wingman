package models

import (
	"gorm.io/gorm"
)

// User 用户模型
type User struct {
	gorm.Model
	Username string `gorm:"uniqueIndex;not null" json:"username"`
	Password string `gorm:"not null" json:"-"`
	Role     string `gorm:"default:user" json:"role"`
}

// Script 脚本模型
type Script struct {
	gorm.Model
	Name        string `gorm:"not null" json:"name"`
	Path        string `gorm:"not null" json:"path"`
	Description string `json:"description"`
	IsRunning   bool   `gorm:"default:false" json:"is_running"`
	Status      string `gorm:"default:stopped" json:"status"`
}

// Settings 配置模型
type Settings struct {
	gorm.Model
	Key   string `gorm:"uniqueIndex;not null" json:"key"`
	Value string `gorm:"type:text" json:"value"`
}

// ExecutionLog 执行日志
type ExecutionLog struct {
	gorm.Model
	ScriptID string `gorm:"index" json:"script_id"`
	Output   string `gorm:"type:text" json:"output"`
	Level    string `json:"level"`
}

// AutoMigrate 自动迁移
func AutoMigrate(db *gorm.DB) error {
	return db.AutoMigrate(
		&User{},
		&Script{},
		&Settings{},
		&ExecutionLog{},
		&Agent{},
		&Workflow{},
		&StepStatus{},
	)
}
