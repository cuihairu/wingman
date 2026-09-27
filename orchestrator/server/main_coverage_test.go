package main

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"testing"
	"time"
	"unsafe"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

func init() { sql.Register("errCloseDriver", errCloseDriver{}) }

// ---------- run() 失败分支 ----------

// chdir 到只读目录后 MkdirAll("./data") 应失败并快速返回错误。
func TestRunCreateDataDirFailure(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("POSIX directory permission bits are not enforced on Windows")
	}
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

// 只读模式打开的既有库：连接成功但 AutoMigrate 写入失败。
func TestRunMigrateFailureOnReadOnlyDB(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("SQLite ?mode=ro does not prevent writes on Windows")
	}
	tmp := t.TempDir()
	dbFile := filepath.Join(tmp, "ro.db")
	if err := os.WriteFile(dbFile, nil, 0o644); err != nil {
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
	t.Setenv("WINGMAN_DB_PATH", "file:"+dbFile+"?mode=ro")

	done := make(chan error, 1)
	go func() { done <- run() }()
	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "failed to migrate database") {
			t.Errorf("expected migrate error, got %v", err)
		}
	case <-time.After(10 * time.Second):
		t.Fatal("run did not return on migrate failure")
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

// errCloseDriver 的连接 Close 恒返回错误：使 database/sql 的 DB.Close
// 汇总连接级错误并返回非 nil，覆盖 shutdownComponents 的错误日志分支。
type errCloseDriver struct{}

type errCloseConn struct{}

func (errCloseDriver) Open(string) (driver.Conn, error)  { return errCloseConn{}, nil }
func (errCloseConn) Prepare(string) (driver.Stmt, error) { return errCloseStmt{}, nil }
func (errCloseConn) Close() error                        { return errors.New("close failed") }
func (errCloseConn) Begin() (driver.Tx, error)           { return nil, errors.New("begin not supported") }
func (errCloseConn) Ping(context.Context) error          { return nil }

type errCloseStmt struct{}

func (errCloseStmt) Close() error                               { return nil }
func (errCloseStmt) NumInput() int                              { return -1 }
func (errCloseStmt) Exec([]driver.Value) (driver.Result, error) { return nil, errors.New("no exec") }
func (errCloseStmt) Query([]driver.Value) (driver.Rows, error)  { return &errCloseRows{}, nil }

type errCloseRows struct{ served bool }

func (r *errCloseRows) Columns() []string { return []string{"version"} }
func (r *errCloseRows) Close() error      { return nil }
func (r *errCloseRows) Next(dest []driver.Value) error {
	if r.served {
		return io.EOF
	}
	r.served = true
	dest[0] = "3.40.0" // 喂给 sqlite dialector 的版本探测
	return nil
}

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
	shutdownComponents(listener, registry, db) // database/sql Close 幂等返回 nil，不应 panic
}

// 连接池持有 Close 报错的连接时，DB.Close 汇总返回该错误 → 覆盖日志分支。
func TestShutdownComponentsCloseError(t *testing.T) {
	hub := ws.NewHub()
	go hub.Run()
	registry := agent.NewRegistry(hub)
	listener := agentPkg.NewFrameListener(registry, hub)

	sqlDB, err := sql.Open("errCloseDriver", "dsn")
	if err != nil {
		t.Fatal(err)
	}
	// 建立一个真实连接并归还空闲池，使 Close 时需要关闭该连接
	if err := sqlDB.Ping(); err != nil {
		t.Fatalf("ping errCloseDriver: %v", err)
	}
	if err := sqlDB.Close(); err == nil {
		t.Fatal("expected DB.Close to propagate connection close error")
	}

	// 用同一 driver 再建一个池（前一个已关闭），注入 gorm 供 shutdownComponents 使用
	sqlDB2, err := sql.Open("errCloseDriver", "dsn-2")
	if err != nil {
		t.Fatal(err)
	}
	if err := sqlDB2.Ping(); err != nil {
		t.Fatal(err)
	}
	db, err := gorm.Open(sqlite.Dialector{Conn: sqlDB2}, &gorm.Config{})
	if err != nil {
		t.Fatalf("open gorm with injected pool: %v", err)
	}
	shutdownComponents(listener, registry, db) // Close 返回 err → 覆盖日志分支
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

// waitTCPUp 等待 addr 可拨通并返回这条连接，超时窗口每次调用独立起算、到点
// 直接 Fatal 收口。两个约束都是 TestRunHTTPEndpointsAndScriptOutput 在 2026-09-27
// 负载下 SIGSEGV 换来的教训：就绪等待把共享 deadline 耗完后，后续拨号循环体
// 一次都不执行，连 err 都不会赋值（留在循环外的还是旧值 nil），nil conn 一路
// 漏到 writeAgentFrame 才炸，崩掉的栈完全看不出真根因；而就绪等待自身不收口的
// 话，server 没起来时后续请求只产生 connection refused 的 Errorf 假信号。
func waitTCPUp(t *testing.T, name, addr string, timeout time.Duration) net.Conn {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
		if err == nil {
			return conn
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("%s %s 在 %s 内未就绪", name, addr, timeout)
	return nil
}

// 完整启动 run() 后：
//  1. GET / 与 GET /ws（无升级头）覆盖静态路由与 WS 路由 handler；
//  2. 模拟 runtime 连接并推送 script_output 事件（缺省 level 与显式 level 两个分支），
//     验证回调把 ExecutionLog 落库；
//  3. SIGINT 优雅关闭。
func TestRunHTTPEndpointsAndScriptOutput(t *testing.T) {
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

	// 等待 HTTP server 就绪：超时 Fatal 收口，不能带着「可能没起来」的状态往下
	// 走产生假信号。窗口 60s 只在失败时才耗满——race 构建在 load 60+ 的共享机
	// 上实测启动要 30s 上下（gin 路由注册到 seed 完成之间被调度饿死），15s 挡不
	// 住；就绪即返回，绿路径零成本。
	base := fmt.Sprintf("http://127.0.0.1:%d", httpPort)
	waitTCPUp(t, "http server", "127.0.0.1:"+strconv.Itoa(httpPort), 60*time.Second).Close()

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
	agentConn := waitTCPUp(t, "frame listener", agentAddr, 15*time.Second)
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
	// 窗口必须是先算好的 deadline：写成 time.Now().Before(time.Now().Add(...))
	// 恒真，事件一旦不落库就挂到测试全局超时，而不是给出干净的失败信息。
	// 15s 同样是失败上界：负载峰值下回调落库也排队，5s 挡不住。
	dbDeadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(dbDeadline) {
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

	// SIGINT 优雅关闭（Windows 上此处 Skip：前述 HTTP/落库断言已执行完毕）
	signalSelfForShutdown(t)
	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("graceful shutdown should return nil, got %v", err)
		}
	case <-time.After(30 * time.Second):
		t.Fatal("run did not return after SIGINT")
	}
}
