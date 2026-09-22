package handlers

// 跨 handler 文件共享的通用辅助函数。仅收纳被多个域文件使用的函数；
// 单文件私有的辅助留在各自文件，横切领域函数（如 WriteAuditLog）留在所属域。

import (
	"fmt"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
)

// parsePositiveInt 解析正整数查询参数，空值或非法值回退 fallback。
// 分页类端点（audit/messages/users 分页）共用。
func parsePositiveInt(value string, fallback int) int {
	if strings.TrimSpace(value) == "" {
		return fallback
	}
	var result int
	_, err := fmt.Sscanf(value, "%d", &result)
	if err != nil || result <= 0 {
		return fallback
	}
	return result
}

// actorName 从请求上下文取当前操作者（用户名 + ID），用于审计/操作日志。
func actorName(c *gin.Context) (string, uint) {
	_, username, _ := middleware.GetCurrentUser(c)
	userID, _ := c.Get("user_id")
	id, _ := userID.(uint)
	return username, id
}

// isUniqueConstraint 判断数据库错误是否为唯一约束冲突（SQLite / MySQL 两种表述）。
func isUniqueConstraint(err error) bool {
	if err == nil {
		return false
	}
	msg := err.Error()
	return strings.Contains(msg, "UNIQUE constraint") || strings.Contains(msg, "Duplicate entry")
}
