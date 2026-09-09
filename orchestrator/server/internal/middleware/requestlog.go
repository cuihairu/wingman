package middleware

import (
	"fmt"

	"github.com/gin-gonic/gin"
)

// RequestLogFormatter 请求日志格式化器，供 gin.LoggerWithFormatter 使用。
// 输出格式：
//
//	[INFO] 2024-01-01 12:00:00 | 200 | 15ms | GET /api/v1/agents
//
// 耗时为 time.Duration 标准字符串（亚毫秒显示 µs，秒级显示 s）。
func RequestLogFormatter(param gin.LogFormatterParams) string {
	return fmt.Sprintf("[INFO] %s | %d | %v | %s %s\n",
		param.TimeStamp.Format("2006-01-02 15:04:05"),
		param.StatusCode,
		param.Latency,
		param.Method,
		param.Path,
	)
}
