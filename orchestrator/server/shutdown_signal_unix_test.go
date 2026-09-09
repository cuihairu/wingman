//go:build !windows

package main

import (
	"os"
	"testing"
)

// signalSelfForShutdown 向本进程发送 SIGINT 触发优雅关闭。
// 仅 Unix 平台：Windows 不支持向进程投递 os.Interrupt 信号。
func signalSelfForShutdown(t *testing.T) {
	t.Helper()
	proc, err := os.FindProcess(os.Getpid())
	if err != nil {
		t.Fatal(err)
	}
	if err := proc.Signal(os.Interrupt); err != nil {
		t.Fatal(err)
	}
}
