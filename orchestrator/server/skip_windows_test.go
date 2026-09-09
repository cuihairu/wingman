//go:build windows

package main

import "testing"

// skipOnWindows 在 Windows 上跳过当前测试。
// 用于 SIGINT 优雅关闭测试：必须在函数入口调用，
// 否则服务器 goroutine 泄漏导致 TempDir 清理失败（file in use）。
func skipOnWindows(t *testing.T) {
	t.Helper()
	t.Skip("process interrupt signal is not supported on Windows")
}
