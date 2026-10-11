package models

import "gorm.io/gorm"

// UpdateRelease 自动更新发布记录（ROADMAP M8.1 自动更新）：
// admin 发布制品（multipart 上传安装包）→ DB 存元数据 + sha256，
// 制品文件本体落 UpdatesDir/<channel>/<platform>-<arch>/。
// runtime/客户端经公开 GET /api/v1/update/latest 取最新 manifest、
// GET /api/v1/update/download/:id 拉制品（只暴露版本号与校验值，无敏感数据）。
// 同一 channel+platform+arch+version 重复发布按 409 拒绝（版本不可变，
// 避免已分发的 sha256 失效）。
type UpdateRelease struct {
	gorm.Model
	// Channel 发布通道（stable/beta/nightly）；查询默认 stable。
	Channel string `gorm:"size:32;index;not null" json:"channel"`
	// Platform 目标平台（windows/macos/linux）。
	Platform string `gorm:"size:32;index;not null" json:"platform"`
	// Arch 目标架构（amd64/arm64）。
	Arch string `gorm:"size:32;index;not null" json:"arch"`
	// Version 语义化版本（x.y.z[-后缀]，如 1.2.3、1.2.3-beta.1）。
	Version string `gorm:"size:64;index;not null" json:"version"`
	// FileName 制品文件名（落盘名，服务端做路径安全校验）。
	FileName string `gorm:"size:256;not null" json:"fileName"`
	// Size 字节数（服务端实测，不信任客户端声明）。
	Size int64 `gorm:"not null;default:0" json:"size"`
	// Sha256 sha256 hex（64 字符，服务端实测）。
	Sha256 string `gorm:"size:64;not null" json:"sha256"`
	// Notes 发布说明（人可读）。
	Notes string `gorm:"size:2048;not null;default:''" json:"notes"`
	// Mandatory 强制更新标记（客户端据此决定是否允许跳过）。
	Mandatory bool `gorm:"not null;default:false" json:"mandatory"`
	// CreatedBy 发布者（admin 用户名），审计用。
	CreatedBy string `gorm:"size:128;not null;default:''" json:"createdBy"`
}
