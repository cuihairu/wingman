package scripts

import (
	"fmt"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestNewStoreCleansRoot(t *testing.T) {
	s := NewStore("/tmp/scripts/")
	if s.Root() != "/tmp/scripts" {
		t.Errorf("expected cleaned root, got %q", s.Root())
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
		{"absolute", "/etc/passwd.lua"},
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
