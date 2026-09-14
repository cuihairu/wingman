package main

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// envMainChild 标记测试二进制作为 main() 子进程被重执行（见 TestMain）。
const envMainChild = "WINGMAN_TEST_MAIN_CHILD"

// TestMain 支持子进程模式：设置 envMainChild=1 时直接运行 main() 本体，
// 用于覆盖 main() 的 log.Fatalf 分支（main 无法在同一进程内测试——
// Fatalf 会终止测试进程）。普通模式下照常运行测试集。
func TestMain(m *testing.M) {
	if os.Getenv(envMainChild) == "1" {
		main()
		return
	}
	os.Exit(m.Run())
}

// findGoCoverDir 从本测试进程的参数中提取 -test.gocoverdir= 指定的覆盖率
// 输出目录（go test -cover 运行时才有）。子进程写同一目录，覆盖率可合并。
func findGoCoverDir() string {
	for _, arg := range os.Args {
		if strings.HasPrefix(arg, "-test.gocoverdir=") {
			return strings.TrimPrefix(arg, "-test.gocoverdir=")
		}
	}
	return ""
}

// TestMainFatalsOnRunError 重执行测试二进制直接跑 main()：
// 无效 JWT secret 使 run() 快速返回配置错误，main() 应 log.Fatalf 以
// exit code 1 退出且 stderr 含错误前缀（覆盖 main() 的 Fatalf 分支）。
func TestMainFatalsOnRunError(t *testing.T) {
	exe, err := os.Executable()
	if err != nil {
		t.Fatalf("resolve test binary: %v", err)
	}

	gocoverdir := findGoCoverDir()
	if gocoverdir == "" {
		gocoverdir = t.TempDir() // 无 -cover 运行时也安全（hooks 需要非空目录）
	}

	cmd := exec.Command(exe)
	cmd.Env = append(os.Environ(),
		envMainChild+"=1",
		"WINGMAN_JWT_SECRET=short", // < 32 字符 → run() 返回 invalid configuration
		"GOCOVERDIR="+gocoverdir,
	)
	var stderr bytes.Buffer
	cmd.Stderr = &stderr

	err = cmd.Run()
	if err == nil {
		t.Fatalf("main should exit non-zero on run error, stderr: %s", stderr.String())
	}
	var exitErr *exec.ExitError
	if !errors.As(err, &exitErr) {
		t.Fatalf("expected ExitError, got %v", err)
	}
	if code := exitErr.ExitCode(); code != 1 {
		t.Errorf("expected exit code 1, got %d", code)
	}
	if !strings.Contains(stderr.String(), "server exited with error") {
		t.Errorf("stderr should contain fatal message, got: %s", stderr.String())
	}
}

// ---------- waitHTTPServer 各分支 ----------

// waitTestComponents 构造 waitHTTPServer 所需的真实组件（listener/registry/db）。
func waitTestComponents(t *testing.T) (*agentPkg.FrameListener, *agent.Registry, *gorm.DB) {
	t.Helper()
	hub := ws.NewHub()
	go hub.Run()
	registry := agent.NewRegistry(hub)
	listener := agentPkg.NewFrameListener(registry, hub)
	dsn := fmt.Sprintf("file:wait_cov_%d?mode=memory&cache=shared", time.Now().UnixNano())
	db, err := gorm.Open(sqlite.Open(dsn), &gorm.Config{})
	if err != nil {
		t.Fatal(err)
	}
	return listener, registry, db
}

// HTTP 启动失败（errCh 返回错误）：应包装为 "failed to start server" 并清理组件。
func TestWaitHTTPServerStartError(t *testing.T) {
	listener, registry, db := waitTestComponents(t)

	errCh := make(chan error, 1)
	errCh <- errors.New("listen tcp 127.0.0.1:1: bind: address already in use")

	err := waitHTTPServer(context.Background(), &http.Server{}, errCh, listener, registry, db)
	if err == nil || !strings.Contains(err.Error(), "failed to start server") {
		t.Fatalf("expected start failure error, got %v", err)
	}
}

// errCh 返回 nil（对应 srv 返回 ErrServerClosed 被 run() 归一化的语义）：
// waitHTTPServer 应返回 nil。
func TestWaitHTTPServerReturnsNilAfterServerClosed(t *testing.T) {
	listener, registry, db := waitTestComponents(t)

	errCh := make(chan error, 1)
	errCh <- nil

	if err := waitHTTPServer(context.Background(), &http.Server{}, errCh, listener, registry, db); err != nil {
		t.Fatalf("expected nil, got %v", err)
	}
}

// blockingListener 的 Accept 在首个连接前阻塞，Close 解除该阻塞并强制返回
// 错误：srv.Shutdown 会先关闭 listener 再等待所有 Accept 循环退出
//（listenerGroup.Wait），因此 Close 必须让 Accept 返回，否则 Shutdown 死锁。
// Close 的错误会被 Shutdown 收集返回，覆盖 waitHTTPServer 的错误日志分支
//（错误仅记录，不影响最终返回 nil）。
type blockingListener struct {
	accepted   chan struct{} // 首次 Accept 调用时关闭
	closed     chan struct{} // Close 时关闭，解除 Accept 阻塞
	acceptOnce sync.Once
	closeOnce  sync.Once
}

func (b *blockingListener) Accept() (net.Conn, error) {
	b.acceptOnce.Do(func() { close(b.accepted) })
	<-b.closed
	return nil, errors.New("listener closed")
}

func (b *blockingListener) Close() error {
	b.closeOnce.Do(func() { close(b.closed) })
	return errors.New("forced close failure")
}

func (b *blockingListener) Addr() net.Addr { return &net.TCPAddr{} }

func TestWaitHTTPServerShutdownError(t *testing.T) {
	listener, registry, db := waitTestComponents(t)

	bl := &blockingListener{accepted: make(chan struct{}), closed: make(chan struct{})}
	srv := &http.Server{Handler: http.NewServeMux()}
	go func() { _ = srv.Serve(bl) }()
	<-bl.accepted // Serve 已注册 listener，Shutdown 才会关闭它

	ctx, cancel := context.WithCancel(context.Background())
	errCh := make(chan error, 1) // 不写入：确保 select 只走 ctx.Done 分支

	done := make(chan error, 1)
	go func() { done <- waitHTTPServer(ctx, srv, errCh, listener, registry, db) }()
	time.Sleep(100 * time.Millisecond) // 让 waitHTTPServer 阻塞在 select
	cancel()

	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("shutdown errors are only logged; expected nil, got %v", err)
		}
	case <-time.After(10 * time.Second):
		t.Fatal("waitHTTPServer did not return after cancel")
	}
}

// ---------- seedRBAC 失败不阻断启动 ----------

// Seed 失败仅记录日志：端口占用场景下 run() 仍应走到 HTTP 绑定并返回
// "failed to start server"（证明种子失败没有中断装配，覆盖其日志分支）。
func TestRunContinuesWhenRBACSeedFails(t *testing.T) {
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

	probe, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	agentPort := probe.Addr().(*net.TCPAddr).Port
	probe.Close()

	oldSeed := seedRBAC
	seedRBAC = func(db *gorm.DB) error { return errors.New("forced seed failure") }
	t.Cleanup(func() { seedRBAC = oldSeed })

	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	t.Setenv("WINGMAN_HOST", "127.0.0.1")
	t.Setenv("WINGMAN_PORT", strconv.Itoa(blockedPort))
	t.Setenv("WINGMAN_DB_PATH", filepath.Join(tmp, "seedfail.db"))
	t.Setenv("WINGMAN_STATIC_DIR", filepath.Join(tmp, "static"))
	t.Setenv("WINGMAN_AGENT_ADDR", "127.0.0.1:"+strconv.Itoa(agentPort))
	t.Setenv("WINGMAN_SCRIPTS_DIR", filepath.Join(tmp, "scripts"))
	t.Setenv("WINGMAN_ADMIN_PASSWORD", "Str0ng!pw")

	done := make(chan error, 1)
	go func() { done <- run() }()
	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "failed to start server") {
			t.Fatalf("seed failure should not abort startup; expected http bind error, got %v", err)
		}
	case <-time.After(30 * time.Second):
		t.Fatal("run did not return")
	}
}
