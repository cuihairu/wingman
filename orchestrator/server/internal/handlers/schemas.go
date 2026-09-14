package handlers

// ErrorResponse 统一错误响应 envelope（所有非 2xx 端点共用）。
type ErrorResponse struct {
	// Success 固定为 false
	Success bool `json:"success" example:"false"`
	// Error 人类可读的错误说明
	Error string `json:"error" example:"insufficient permissions"`
}

// BatchSummaryResponse 批量操作响应 envelope。
type BatchSummaryResponse struct {
	// Success 固定为 true（部分失败不算整体失败，逐台结果见 data.results）
	Success bool `json:"success" example:"true"`
	// Data 批量结果汇总
	Data BatchSummary `json:"data"`
}
