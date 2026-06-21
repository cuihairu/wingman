package models

import (
	"time"

	"gorm.io/gorm"
)

// User 用户模型
type User struct {
	gorm.Model
	Username    string     `gorm:"uniqueIndex;not null" json:"username"`
	Password    string     `gorm:"not null" json:"-"`
	Role        string     `gorm:"default:user;index" json:"role"`
	Active      bool       `json:"active"`
	Nickname    string     `json:"nickname"`
	Email       string     `json:"email"`
	Phone       string     `json:"phone"`
	Avatar      string     `gorm:"type:text" json:"avatar"`
	LastLoginAt *time.Time `json:"lastLoginAt"`
}

// SafeUser 脱敏后的用户视图（不含密码）
type SafeUser struct {
	ID          uint   `json:"id"`
	Username    string `json:"username"`
	Nickname    string `json:"nickname"`
	DisplayName string `json:"displayName"`
	Email       string `json:"email"`
	Phone       string `json:"phone"`
	Avatar      string `json:"avatar"`
	Role        string `json:"role"`
	Active      bool   `json:"active"`
	CreatedAt   string `json:"createdAt"`
	UpdatedAt   string `json:"updatedAt"`
	LastLoginAt string `json:"lastLoginAt"`
}

// ToSafe 返回脱敏视图
func (u User) ToSafe() SafeUser {
	created := ""
	if !u.CreatedAt.IsZero() {
		created = u.CreatedAt.Format("2006-01-02 15:04:05")
	}
	updated := ""
	if !u.UpdatedAt.IsZero() {
		updated = u.UpdatedAt.Format("2006-01-02 15:04:05")
	}
	lastLogin := ""
	if u.LastLoginAt != nil && !u.LastLoginAt.IsZero() {
		lastLogin = u.LastLoginAt.Format("2006-01-02 15:04:05")
	}
	displayName := u.Nickname
	if displayName == "" {
		displayName = u.Username
	}
	return SafeUser{
		ID:          u.ID,
		Username:    u.Username,
		Nickname:    u.Nickname,
		DisplayName: displayName,
		Email:       u.Email,
		Phone:       u.Phone,
		Avatar:      u.Avatar,
		Role:        u.Role,
		Active:      u.Active,
		CreatedAt:   created,
		UpdatedAt:   updated,
		LastLoginAt: lastLogin,
	}
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

// AuditLog stores operational audit events for dashboard views.
type AuditLog struct {
	gorm.Model
	Actor  string `gorm:"index" json:"actor"`
	Kind   string `gorm:"index;not null" json:"kind"`
	Target string `gorm:"index" json:"target"`
	Meta   string `gorm:"type:text" json:"meta"`
}

// Message stores in-app notifications for dashboard users.
type Message struct {
	gorm.Model
	Recipient string `gorm:"index" json:"recipient"`
	Title     string `gorm:"not null" json:"title"`
	Content   string `gorm:"type:text" json:"content"`
	Status    string `gorm:"index;default:unread" json:"status"`
	Category  string `gorm:"index" json:"category"`
	Source    string `json:"source"`
}

// MessageRead 记录某用户已读某条消息，用于实现 per-user 已读状态。
// 这样广播消息（recipient='*'）的单行被多个用户共享时，
// 每个用户的已读状态相互独立，避免污染。
type MessageRead struct {
	ID        uint `gorm:"primaryKey" json:"id"`
	UserID    uint `gorm:"index:idx_msgread_user,unique" json:"userId"`
	MessageID uint `gorm:"index:idx_msgread_user,unique" json:"messageId"`
	CreatedAt time.Time `json:"createdAt"`
}

func (MessageRead) TableName() string { return "message_reads" }

// Feedback stores support and permission-request submissions.
type Feedback struct {
	gorm.Model
	Actor    string `gorm:"index" json:"actor"`
	Category string `gorm:"index" json:"category"`
	Content  string `gorm:"type:text;not null" json:"content"`
	Priority string `gorm:"index" json:"priority"`
	Source   string `json:"source"`
	Status   string `gorm:"index;default:open" json:"status"`
}

// AutoMigrate 自动迁移
func AutoMigrate(db *gorm.DB) error {
	return db.AutoMigrate(
		&User{},
		&Script{},
		&Settings{},
		&ExecutionLog{},
		&AuditLog{},
		&Message{},
		&MessageRead{},
		&Feedback{},
		&Agent{},
		&Workflow{},
		&StepStatus{},
		&Role{},
		&Permission{},
	)
}
