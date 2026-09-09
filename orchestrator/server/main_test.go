package main

import (
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"testing"
	"time"
)

// run() 走完整装配流程：占住目标 HTTP 端口使 r.Run 绑定失败返回 error，
// 从而在测试中覆盖配置加载、数据库迁移、RBAC 种子、路由注册等全部装配逻辑。
func TestRunFullAssemblyPortConflict(t *testing.T) {
	// 隔离工作目录，避免在仓库内创建 ./data 与 scripts 目录
	tmp := t.TempDir()
	oldWD, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(tmp); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(oldWD) })

	// 占住一个端口，迫使 r.Run 失败
	blocker, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	defer blocker.Close()
	blockedPort := blocker.Addr().(*net.TCPAddr).Port

	// 随机 TCP 端口给 agent listener，避免与其他测试冲突
	probe, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	agentPort := probe.Addr().(*net.TCPAddr).Port
	probe.Close()

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_HOST", "127.0.0.1")
	t.Setenv("WINGMAN_PORT", strconv.Itoa(blockedPort))
	t.Setenv("WINGMAN_DB_PATH", filepath.Join(tmp, "test-wingman.db"))
	t.Setenv("WINGMAN_STATIC_DIR", filepath.Join(tmp, "static"))
	t.Setenv("WINGMAN_AGENT_ADDR", "127.0.0.1:"+strconv.Itoa(agentPort))
	t.Setenv("WINGMAN_SCRIPTS_DIR", filepath.Join(tmp, "scripts"))
	t.Setenv("WINGMAN_ADMIN_PASSWORD", "Str0ng!pw")

	done := make(chan error, 1)
	go func() { done <- run() }()

	select {
	case err := <-done:
		if err == nil {
			t.Fatal("run should fail when the HTTP port is occupied")
		}
		if !strings.Contains(err.Error(), "failed to start server") {
			t.Errorf("unexpected error: %v", err)
		}
	case <-time.After(30 * time.Second):
		t.Fatal("run did not return on port conflict")
	}

	// data 目录应已创建
	if _, err := os.Stat(filepath.Join(tmp, "data")); err != nil {
		t.Errorf("data directory should be created: %v", err)
	}
}

// 无效配置应快速返回错误（不触发后续装配）。
func TestRunInvalidConfig(t *testing.T) {
	tmp := t.TempDir()
	oldWD, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(tmp); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(oldWD) })

	t.Setenv("WINGMAN_JWT_SECRET", "short") // < 32 字符 → 配置错误
	t.Setenv("WINGMAN_DB_PATH", filepath.Join(tmp, "x.db"))

	done := make(chan error, 1)
	go func() { done <- run() }()
	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "invalid configuration") {
			t.Errorf("expected config error, got %v", err)
		}
	case <-time.After(10 * time.Second):
		t.Fatal("run did not return on invalid config")
	}
}

// 完整启动后发送 SIGINT：run() 应优雅关闭 HTTP/agent listener/数据库并返回 nil。
// signal.NotifyContext 拦截 SIGINT，不会终止测试进程本身。
func TestRunGracefulShutdownOnSIGINT(t *testing.T) {
	skipOnWindows(t)
	tmp := t.TempDir()
	oldWD, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(tmp); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(oldWD) })

	pickPort := func() int {
		ln, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			t.Fatal(err)
		}
		port := ln.Addr().(*net.TCPAddr).Port
		ln.Close()
		return port
	}
	httpPort := pickPort()
	agentPort := pickPort()

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_HOST", "127.0.0.1")
	t.Setenv("WINGMAN_PORT", strconv.Itoa(httpPort))
	t.Setenv("WINGMAN_DB_PATH", filepath.Join(tmp, "graceful.db"))
	t.Setenv("WINGMAN_STATIC_DIR", filepath.Join(tmp, "static"))
	t.Setenv("WINGMAN_AGENT_ADDR", "127.0.0.1:"+strconv.Itoa(agentPort))
	t.Setenv("WINGMAN_SCRIPTS_DIR", filepath.Join(tmp, "scripts"))
	t.Setenv("WINGMAN_ADMIN_PASSWORD", "Str0ng!pw")

	done := make(chan error, 1)
	go func() { done <- run() }()

	// 轮询等待 HTTP server 就绪
	deadline := time.Now().Add(15 * time.Second)
	ready := false
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", "127.0.0.1:"+strconv.Itoa(httpPort), 200*time.Millisecond)
		if err == nil {
			conn.Close()
			ready = true
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if !ready {
		t.Fatal("HTTP server did not become ready in time")
	}

	// 发送 SIGINT 触发优雅关闭（Windows 无进程信号机制，整测试跳过）
	signalSelfForShutdown(t)

	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("graceful shutdown should return nil, got: %v", err)
		}
	case <-time.After(30 * time.Second):
		t.Fatal("run did not return after SIGINT")
	}

	// 关闭后端口应不再接受连接
	conn, err := net.DialTimeout("tcp", "127.0.0.1:"+strconv.Itoa(httpPort), 200*time.Millisecond)
	if err == nil {
		conn.Close()
		t.Error("HTTP port should be closed after shutdown")
	}
}
