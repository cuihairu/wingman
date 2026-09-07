package config

import (
	"os"
	"path/filepath"
	"testing"
)

const testSecret = "0123456789abcdef0123456789abcdef"

func TestLoadDefaults(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)
	for _, key := range []string{"WINGMAN_HOST", "WINGMAN_PORT", "WINGMAN_DB_PATH", "WINGMAN_STATIC_DIR", "WINGMAN_AGENT_ADDR", "WINGMAN_SCRIPTS_DIR", "WINGMAN_CORS_ORIGINS"} {
		t.Setenv(key, "")
	}

	cfg, err := Load()
	if err != nil {
		t.Fatalf("load: %v", err)
	}
	if cfg.Host != "127.0.0.1" || cfg.Port != 9527 || cfg.AgentAddr != "127.0.0.1:8888" {
		t.Errorf("unexpected defaults: %+v", cfg)
	}
	if cfg.JWTSecret != testSecret {
		t.Errorf("jwt secret not loaded")
	}
	if cfg.CORSOrigins != nil {
		t.Errorf("expected nil CORS origins, got %v", cfg.CORSOrigins)
	}
	abs, err := filepath.Abs("./scripts")
	if err != nil {
		t.Fatal(err)
	}
	if cfg.ScriptsDir != filepath.Clean(abs) {
		t.Errorf("scripts dir should be abs+clean: got %q want %q", cfg.ScriptsDir, filepath.Clean(abs))
	}
}

func TestLoadOverrides(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)
	t.Setenv("WINGMAN_HOST", "localhost")
	t.Setenv("WINGMAN_PORT", "9000")
	t.Setenv("WINGMAN_CORS_ORIGINS", " http://a.com , ,https://b.com ")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("load: %v", err)
	}
	if cfg.Host != "localhost" || cfg.Port != 9000 {
		t.Errorf("overrides not applied: %+v", cfg)
	}
	if len(cfg.CORSOrigins) != 2 || cfg.CORSOrigins[0] != "http://a.com" || cfg.CORSOrigins[1] != "https://b.com" {
		t.Errorf("CORS origins parsing failed: %v", cfg.CORSOrigins)
	}
}

func TestLoadInvalidValues(t *testing.T) {
	cases := []struct {
		name string
		env  map[string]string
	}{
		{"missing secret", map[string]string{"WINGMAN_JWT_SECRET": ""}},
		{"short secret", map[string]string{"WINGMAN_JWT_SECRET": "tooshort"}},
		{"bad host", map[string]string{
			"WINGMAN_JWT_SECRET": testSecret,
			"WINGMAN_HOST":       "not-an-ip",
		}},
		{"port zero", map[string]string{
			"WINGMAN_JWT_SECRET": testSecret,
			"WINGMAN_PORT":       "0",
		}},
		{"port too large", map[string]string{
			"WINGMAN_JWT_SECRET": testSecret,
			"WINGMAN_PORT":       "65536",
		}},
		{"port negative", map[string]string{
			"WINGMAN_JWT_SECRET": testSecret,
			"WINGMAN_PORT":       "-1",
		}},
	}
	// 每轮先清空所有相关变量，避免上一个 case 的残留干扰
	resetKeys := []string{"WINGMAN_JWT_SECRET", "WINGMAN_HOST", "WINGMAN_PORT"}
	for _, tc := range cases {
		for _, k := range resetKeys {
			t.Setenv(k, "")
		}
		for k, v := range tc.env {
			t.Setenv(k, v)
		}
		if _, err := Load(); err == nil {
			t.Errorf("%s: expected error", tc.name)
		}
	}
}

func TestLoadInvalidPortFallsBack(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)
	t.Setenv("WINGMAN_PORT", "not-a-number")
	cfg, err := Load()
	if err != nil {
		t.Fatalf("non-numeric port should fall back: %v", err)
	}
	if cfg.Port != 9527 {
		t.Errorf("expected fallback port, got %d", cfg.Port)
	}
}

func TestLoadScriptsDirAbsFailsWithoutCwd(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)

	// 删除当前工作目录后 filepath.Abs（依赖 os.Getwd）失败
	orig, err := os.Getwd()
	if err != nil {
		t.Skipf("cannot get cwd: %v", err)
	}
	deleted := t.TempDir()
	if err := os.Chdir(deleted); err != nil {
		t.Skipf("cannot chdir: %v", err)
	}
	if err := os.Remove(deleted); err != nil {
		t.Fatalf("remove dir: %v", err)
	}
	defer func() { _ = os.Chdir(orig) }()

	if _, err := Load(); err == nil {
		t.Error("expected error when cwd is unavailable")
	}
}

func TestMustLoad(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)
	if cfg := MustLoad(); cfg.JWTSecret != testSecret {
		t.Error("MustLoad should return config")
	}

	t.Setenv("WINGMAN_JWT_SECRET", "")
	defer func() {
		if recover() == nil {
			t.Error("MustLoad should panic on invalid config")
		}
	}()
	MustLoad()
}

func TestAddr(t *testing.T) {
	got := Addr(Config{Host: "10.1.2.3", Port: 9999})
	if got != "10.1.2.3:9999" {
		t.Errorf("unexpected addr: %q", got)
	}
}

func TestSplitList(t *testing.T) {
	if got := splitList(""); got != nil {
		t.Errorf("empty input should return nil, got %v", got)
	}
	if got := splitList("  "); len(got) != 0 {
		t.Errorf("blank input should return empty, got %v", got)
	}
	got := splitList("a, b ,,c")
	if len(got) != 3 || got[0] != "a" || got[1] != "b" || got[2] != "c" {
		t.Errorf("unexpected split: %v", got)
	}
}
