package scripts

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestNewStoreCleansRoot(t *testing.T) {
	// 用平台相关分隔符构造输入，避免硬编码 Unix 路径（Windows 上 Clean 会得到 \tmp\scripts）
	rel := NewStore(filepath.Join("scripts", "root") + string(filepath.Separator))
	if rel.Root() != filepath.Clean(filepath.Join("scripts", "root")) {
		t.Errorf("relative root not cleaned: got %q", rel.Root())
	}

	tmp := t.TempDir()
	if got := NewStore(tmp + string(filepath.Separator)).Root(); got != filepath.Clean(tmp) {
		t.Errorf("absolute root not cleaned: got %q want %q", got, filepath.Clean(tmp))
	}
}

func TestResolveValid(t *testing.T) {
	root := t.TempDir()
	s := NewStore(root)

	got, err := s.Resolve("hello.lua")
	if err != nil {
		t.Fatalf("resolve: %v", err)
	}
	want := filepath.Join(root, "hello.lua")
	if got != want {
		t.Errorf("got %q, want %q", got, want)
	}

	// 子目录路径合法
	got, err = s.Resolve("sub/dir/run.lua")
	if err != nil {
		t.Fatalf("resolve nested: %v", err)
	}
	if got != filepath.Join(root, "sub", "dir", "run.lua") {
		t.Errorf("unexpected nested path: %q", got)
	}
}

func TestResolveRejectsInvalid(t *testing.T) {
	s := NewStore(t.TempDir())

	cases := []struct {
		name string
		in   string
	}{
		{"empty", ""},
		{"whitespace", "   "},
		// 绝对路径必须用平台相关构造：/etc/passwd.lua 在 Windows 上不是绝对路径，
		// IsAbs 不命中，Resolve 反而合法返回。
		{"absolute", filepath.Join(os.TempDir(), "wingman-abs.lua")},
		{"escape parent", "../outside.lua"},
		{"nested escape", "sub/../../outside.lua"},
		{"non-lua ext", "readme.txt"},
		{"no ext", "readme"},
	}
	for _, tc := range cases {
		if _, err := s.Resolve(tc.in); err == nil {
			t.Errorf("%s: expected error for %q", tc.name, tc.in)
		}
	}
}

func TestResolvePropagatesRelError(t *testing.T) {
	s := NewStore(t.TempDir())
	orig := filepathRel
	defer func() { filepathRel = orig }()
	filepathRel = func(basepath, targpath string) (string, error) {
		return "", fmt.Errorf("injected rel failure")
	}
	if _, err := s.Resolve("x.lua"); err == nil || !strings.Contains(err.Error(), "injected rel failure") {
		t.Fatalf("expected injected rel error, got %v", err)
	}
}

func TestDisplayPath(t *testing.T) {
	root := t.TempDir()
	full := filepath.Join(root, "a", "b.lua")

	got := DisplayPath(root, full)
	if got != "a/b.lua" {
		t.Errorf("expected a/b.lua, got %q", got)
	}

	// Rel 失败时回退为原路径（相对路径混用时触发）
	if got := DisplayPath("relative/root", "/abs/path.lua"); got != "/abs/path.lua" {
		t.Errorf("expected fallback to full path, got %q", got)
	}
	// Windows 分隔符统一为 /
	if runtime.GOOS == "windows" {
		if got := DisplayPath(`C:\r`, `C:\r\x.lua`); got != "x.lua" {
			t.Errorf("expected slash-normalized path, got %q", got)
		}
	}
}
