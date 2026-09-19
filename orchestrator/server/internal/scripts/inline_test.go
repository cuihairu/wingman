package scripts

import (
	"errors"
	"os"
	"path/filepath"
	"testing"
)

func TestReadInline(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "hello.lua")
	if err := os.WriteFile(path, []byte("print('hi')"), 0o644); err != nil {
		t.Fatal(err)
	}
	content, err := ReadInline(path)
	if err != nil {
		t.Fatalf("read: %v", err)
	}
	if string(content) != "print('hi')" {
		t.Errorf("unexpected content: %q", content)
	}
}

func TestReadInlineMissing(t *testing.T) {
	_, err := ReadInline(filepath.Join(t.TempDir(), "gone.lua"))
	if err == nil || errors.Is(err, ErrScriptTooLarge) {
		t.Errorf("missing file should be a plain read error, got %v", err)
	}
}

func TestReadInlineTooLarge(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "big.lua")
	big := make([]byte, MaxInlineScriptSize+1)
	if err := os.WriteFile(path, big, 0o644); err != nil {
		t.Fatal(err)
	}
	if _, err := ReadInline(path); !errors.Is(err, ErrScriptTooLarge) {
		t.Errorf("oversize should wrap ErrScriptTooLarge, got %v", err)
	}
}

func TestLanguageOf(t *testing.T) {
	cases := map[string]string{
		"a.lua":     "lua",
		"A.LUA":     "lua",
		"b.py":      "python",
		"c.txt":     "",
		"d":         "",
		"dir/e.lua": "lua",
	}
	for path, want := range cases {
		if got := LanguageOf(path); got != want {
			t.Errorf("LanguageOf(%q) = %q, want %q", path, got, want)
		}
	}
}
