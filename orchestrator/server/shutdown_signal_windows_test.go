//go:build windows

package main

import "testing"

// signalSelfForShutdown 在 Windows 上跳过：os.Process.Signal(os.Interrupt)
// 不被 Windows 支持（无进程信号机制），SIGINT 优雅关闭链路仅在 Unix 验证。
func signalSelfForShutdown(t *testing.T) {
	t.Helper()
	t.Skip("process interrupt signal is not supported on Windows")
}
