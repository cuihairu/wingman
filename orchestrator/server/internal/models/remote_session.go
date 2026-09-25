package models

import (
	"time"

	"gorm.io/gorm"
)

// RemoteSessionAudit 远程桌面会话审计（设计 §8 安全模型「审计」条 +
// §11 P1「审计报表呈现」）。
//
// 为什么独立于 AuditLog：通用审计表是「事件流水」（append-only，一条
// 事件一行，meta 是 JSON），适合回答「谁在什么时候做了什么」；而报表要
// 回答的是「这台机器这个月被谁接管了多久、录了几段、失败几次」——那需要
// **可聚合的结构化列**（协议/模式/时长/录像名），在 JSON meta 上做聚合
// 既慢又脆。故另立专表，AuditLog 侧的四事件审计（票据/连接/断开/失败）
// 保持不变——两者受众不同：审计流水给合规逐条查，报表给运维看趋势。
//
// 安全性：不含任何凭证与画面内容（密码只在网关→guacd 的 connect 指令里，
// 从不落库）；host/port 属内网端点信息，与既有 desktop.* 审计同源同权限。
type RemoteSessionAudit struct {
	gorm.Model
	// SessionID 网关侧会话 ID（newGuacSessionID 生成的 hex），与录像文件名
	// 同源——{agentID}-{sessionID}.mjs，故录像与报表可双向关联。
	SessionID string `gorm:"index;not null" json:"sessionId"`
	// AgentID 目标 agent（索引，报表按机器聚合）
	AgentID string `gorm:"index;not null" json:"agentId"`
	// Operator 操作者用户名（索引，报表按人聚合）
	Operator string `gorm:"index;not null" json:"operator"`
	// Protocol rdp/vnc/ssh（索引，报表按协议聚合）
	Protocol string `gorm:"index;not null" json:"protocol"`
	// Host / Port agent 侧 endpoint（内网地址，非公网暴露面）
	Host string `json:"host"`
	Port int    `json:"port"`
	// ReadOnly true 监看 / false 接管（索引，报表要区分「看」与「动」）
	ReadOnly bool `gorm:"index" json:"readOnly"`
	// Record 是否录制（索引）
	Record bool `gorm:"index" json:"record"`
	// RecordingName 录像文件名（record=true 时；与录像检索 API 的 name 同值，
	// 可直接跳转到该录像）
	RecordingName string `json:"recordingName,omitempty"`
	// Status closed=正常断开 / failed=建连失败（索引；进行中的会话不落行，
	// 断开时一次性写入——避免报表把进行中会话算进时长）
	Status string `gorm:"index;not null" json:"status"`
	// FailReason 建连失败原因（如 guacd unreachable），成功会话为空
	FailReason string `json:"failReason,omitempty"`
	// StartedAt / EndedAt 会话起止（StartedAt 索引支持时间范围查询）
	StartedAt time.Time  `gorm:"index" json:"startedAt"`
	EndedAt   *time.Time `json:"endedAt"`
	// DurationMs 会话时长（毫秒；报表求和列，避免报表端算时差）
	DurationMs int64 `json:"durationMs"`
}

func (RemoteSessionAudit) TableName() string { return "remote_session_audits" }

// 会话状态取值。刻意只有两个终态：进行中的会话不落行（见字段注释）。
const (
	// RemoteSessionStatusClosed 正常断开
	RemoteSessionStatusClosed = "closed"
	// RemoteSessionStatusFailed 建连失败（连 guacd 都拨不通，无像素面）
	RemoteSessionStatusFailed = "failed"
)
