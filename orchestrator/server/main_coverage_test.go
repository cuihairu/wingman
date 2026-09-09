package main

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	
	"testing"
	"time"
	"unsafe"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// ---------- run() 失败分支 ----------

// chdir 到只读目录后 MkdirAll("./data") 应失败并快速返回错误。
func TestRunCreateDataDirFailure(t *testing.T) {
	if os.Geteuid() == 0 {
		t.Skip("root ignores directory permission bits")
	}
	ro := t.TempDir()
	if err := os.Chmod(ro, 0o500); err != nil {
		t.Fatal(err)
	}
	oldWD, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(ro); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(oldWD) })
	t.Cleanup(func() { os.Chmod(ro, 0o700) })

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_DB_PATH", filepath.Join(ro, "x.db"))

	done := make(chan error, 1)
	go func() { done <- run() }()
	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "failed to create data directory") {
			t.Errorf("expected data dir error, got %v", err)
		}
	case <-time.After(10 * time.Second):
		t.Fatal("run did not return on data dir failure")
	}
}

// DBPath 指向目录时 gorm.Open 应失败。
func TestRunDBOpenFailure(t *testing.T) {
	tmp := t.TempDir()
	dbDir := filepath.Join(tmp, "db-as-dir")
	if err := os.Mkdir(dbDir, 0o755); err != nil {
		t.Fatal(err)
	}
	oldWD, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(tmp); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(oldWD) })

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_DB_PATH", dbDir)

	done := make(chan error, 1)
	go func() { done <- run() }()
	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "failed to connect database") {
			t.Errorf("expected db connect error, got %v", err)
		}
	case <-time.After(10 * time.Second):
		t.Fatal("run did not return on db open failure")
	}
}

// agent listener 端口与 HTTP 端口同时被占：listener Start 失败仅记录日志，
// run() 因 HTTP 绑定失败返回错误。
func TestRunFrameListenerPortConflict(t *testing.T) {
	tmp := t.TempDir()
	oldWD, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(tmp); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(oldWD) })

	blocker, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	defer blocker.Close()
	blockedPort := blocker.Addr().(*net.TCPAddr).Port

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_HOST", "127.0.0.1")
	t.Setenv("WINGMAN_PORT", strconv.Itoa(blockedPort))
	t.Setenv("WINGMAN_DB_PATH", filepath.Join(tmp, "conflict.db"))
	t.Setenv("WINGMAN_STATIC_DIR", filepath.Join(tmp, "static"))
	t.Setenv("WINGMAN_AGENT_ADDR", "127.0.0.1:"+strconv.Itoa(blockedPort))
	t.Setenv("WINGMAN_SCRIPTS_DIR", filepath.Join(tmp, "scripts"))
	t.Setenv("WINGMAN_ADMIN_PASSWORD", "Str0ng!pw")

	done := make(chan error, 1)
	go func() { done <- run() }()
	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "failed to start server") {
			t.Errorf("expected http bind error, got %v", err)
		}
	case <-time.After(30 * time.Second):
		t.Fatal("run did not return on port conflict")
	}
	// 给 listener goroutine 时间执行并记录 Start 失败日志（覆盖该分支）
	time.Sleep(300 * time.Millisecond)
}

// ---------- shutdownComponents ----------

// 二次关闭同一 db：Close 返回错误并记录日志（覆盖错误分支），组件 Stop 幂等安全。
func TestShutdownComponentsDBError(t *testing.T) {
	hub := ws.NewHub()
	go hub.Run()
	registry := agent.NewRegistry(hub)
	listener := agentPkg.NewFrameListener(registry, hub)

	dsn := fmt.Sprintf("file:shutdown_cov_%d?mode=memory&cache=shared", time.Now().UnixNano())
	db, err := gorm.Open(sqlite.Open(dsn), &gorm.Config{})
	if err != nil {
		t.Fatal(err)
	}

	shutdownComponents(listener, registry, db) // 正常路径
	shutdownComponents(listener, registry, db) // Close 已关闭的连接 → 错误日志分支
}

// ---------- HTTP 端点与脚本输出回调 ----------

var frameEndian = func() binary.ByteOrder {
	var x uint16 = 0x0102
	if *(*byte)(unsafe.Pointer(&x)) == 0x02 {
		return binary.LittleEndian
	}
	return binary.BigEndian
}()

// writeAgentFrame 向 orchestrator 的 FrameListener 写一帧 Notify 消息。
func writeAgentFrame(t *testing.T, conn net.Conn, payload map[string]any) {
	t.Helper()
	body, err := json.Marshal(payload)
	if err != nil {
		t.Fatal(err)
	}
	header := make([]byte, 16)
	frameEndian.PutUint32(header[0:4], uint32(len(body)))
	header[8] = 3 // Notify
	if _, err := conn.Write(header); err != nil {
		t.Fatal(err)
	}
	if _, err := conn.Write(body); err != nil {
		t.Fatal(err)
	}
}

// 完整启动 run() 后：
//  1. GET / 与 GET /ws（无升级头）覆盖静态路由与 WS 路由 handler；
//  2. 模拟 runtime 连接并推送 script_output 事件（缺省 level 与显式 level 两个分支），
//     验证回调把 ExecutionLog 落库；
//  3. SIGINT 优雅关闭。
func TestRunHTTPEndpointsAndScriptOutput(t *testing.T) {
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
	dbPath := filepath.Join(tmp, "main-cov.db")

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_HOST", "127.0.0.1")
	t.Setenv("WINGMAN_PORT", strconv.Itoa(httpPort))
	t.Setenv("WINGMAN_DB_PATH", dbPath)
	t.Setenv("WINGMAN_STATIC_DIR", filepath.Join(tmp, "static"))
	t.Setenv("WINGMAN_AGENT_ADDR", "127.0.0.1:"+strconv.Itoa(agentPort))
	t.Setenv("WINGMAN_SCRIPTS_DIR", filepath.Join(tmp, "scripts"))
	t.Setenv("WINGMAN_ADMIN_PASSWORD", "Str0ng!pw")

	done := make(chan error, 1)
	go func() { done <- run() }()

	// 等待 HTTP server 就绪
	base := fmt.Sprintf("http://127.0.0.1:%d", httpPort)
	deadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", "127.0.0.1:"+strconv.Itoa(httpPort), 200*time.Millisecond)
		if err == nil {
			conn.Close()
			break
		}
		time.Sleep(50 * time.Millisecond)
	}

	// GET / → c.File handler（index.html 缺失返回 404，handler 已执行）
	if resp, err := http.Get(base + "/"); err != nil {
		t.Errorf("GET /: %v", err)
	} else {
		io.Copy(io.Discard, resp.Body)
		resp.Body.Close()
	}
	// GET /ws（无升级头）→ upgrader 拒绝，handler 已执行
	if resp, err := http.Get(base + "/ws"); err != nil {
		t.Errorf("GET /ws: %v", err)
	} else {
		io.Copy(io.Discard, resp.Body)
		resp.Body.Close()
	}

	// 模拟 runtime：连接 FrameListener，注册并推送两条 script_output（覆盖默认/显式 level）
	agentAddr := "127.0.0.1:" + strconv.Itoa(agentPort)
	var agentConn net.Conn
	for time.Now().Before(deadline) {
		agentConn, err = net.DialTimeout("tcp", agentAddr, 200*time.Millisecond)
		if err == nil {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if err != nil {
		t.Fatalf("dial frame listener: %v", err)
	}
	defer agentConn.Close()

	writeAgentFrame(t, agentConn, map[string]any{"type": "agent.register", "agentId": "cov-agent", "hostname": "cov-host"})
	writeAgentFrame(t, agentConn, map[string]any{
		"type": "agent.event", "event": "script_output",
		"data": map[string]any{"scriptId": "cov1", "message": "default level"},
	})
	writeAgentFrame(t, agentConn, map[string]any{
		"type": "agent.event", "event": "script_output",
		"data": map[string]any{"scriptId": "cov2", "message": "explicit level", "level": "warn"},
	})

	// 轮询验证回调已把 ExecutionLog 落库
	reader, err := gorm.Open(sqlite.Open(dbPath), &gorm.Config{})
	if err != nil {
		t.Fatalf("open db for verification: %v", err)
	}
	ok := false
	for time.Now().Before(time.Now().Add(5 * time.Second)) {
		var logs []models.ExecutionLog
		if err := reader.Where("script_id IN ?", []string{"cov1", "cov2"}).Order("script_id").Find(&logs).Error; err == nil &&
			len(logs) == 2 && logs[0].Level == "info" && logs[1].Level == "warn" {
			ok = true
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if !ok {
		t.Error("script_output events were not persisted with expected levels")
	}

	// SIGINT 优雅关闭
	proc, err := os.FindProcess(os.Getpid())
	if err != nil {
		t.Fatal(err)
	}
	if err := proc.Signal(os.Interrupt); err != nil {
		t.Fatal(err)
	}
	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("graceful shutdown should return nil, got %v", err)
		}
	case <-time.After(30 * time.Second):
		t.Fatal("run did not return after SIGINT")
	}
}
