package handlers

// ErrorResponse 统一错误响应 envelope（所有非 2xx 端点共用）。
type ErrorResponse struct {
	// Success 固定为 false
	Success bool `json:"success" example:"false"`
	// Error 人类可读的错误说明
	Error string `json:"error" example:"insufficient permissions"`
}
