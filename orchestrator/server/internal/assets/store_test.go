package assets

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
)

func TestStoreSaveListGetDelete(t *testing.T) {
	store := NewStore(filepath.Join(t.TempDir(), "assets"))

	meta, err := store.Save("login_button.png", []byte("fake-png-bytes"))
	if err != nil {
		t.Fatalf("save: %v", err)
	}
	if meta.Name != "login_button.png" || meta.Size != int64(len("fake-png-bytes")) {
		t.Errorf("unexpected meta: %+v", meta)
	}
	if len(meta.Sha256) != 64 {
		t.Errorf("sha256 should be 64 hex chars, got %q", meta.Sha256)
	}

	metas, err := store.List()
	if err != nil || len(metas) != 1 || metas[0].Name != "login_button.png" {
		t.Fatalf("list: %+v err=%v", metas, err)
	}

	content, got, err := store.Get("login_button.png")
	if err != nil || string(content) != "fake-png-bytes" || got.Sha256 != meta.Sha256 {
		t.Fatalf("get: content=%q meta=%+v err=%v", content, got, err)
	}

	// 同名覆盖：内容与 sha256 更新
	meta2, err := store.Save("login_button.png", []byte("v2"))
	if err != nil {
		t.Fatalf("overwrite: %v", err)
	}
	if meta2.Sha256 == meta.Sha256 || meta2.Size != 2 {
		t.Errorf("overwrite should update sha256/size: %+v", meta2)
	}
	content, _, err = store.Get("login_button.png")
	if err != nil || string(content) != "v2" {
		t.Fatalf("get after overwrite: %q err=%v", content, err)
	}

	if err := store.Delete("login_button.png"); err != nil {
		t.Fatalf("delete: %v", err)
	}
	if _, _, err := store.Get("login_button.png"); !errors.Is(err, ErrAssetNotFound) {
		t.Errorf("get after delete should be ErrAssetNotFound, got %v", err)
	}
	if err := store.Delete("login_button.png"); !errors.Is(err, ErrAssetNotFound) {
		t.Errorf("delete again should be ErrAssetNotFound, got %v", err)
	}
}

func TestStoreListEmptyDirMissing(t *testing.T) {
	store := NewStore(filepath.Join(t.TempDir(), "never-created"))
	metas, err := store.List()
	if err != nil || len(metas) != 0 {
		t.Fatalf("list on missing dir: %+v err=%v", metas, err)
	}
}

func TestSanitizeName(t *testing.T) {
	valid := []string{"a.png", "btn_2.png", "A-B.c.d", "x" + strings.Repeat("y", 127)}
	for _, name := range valid {
		if _, err := SanitizeName(name); err != nil {
			t.Errorf("%q should be valid, got %v", name, err)
		}
	}
	invalid := []string{
		"", ".hidden", "/etc/passwd", "../escape", "a/b.png", "a\\b.png",
		"with space.png", "with\x00nul.png", strings.Repeat("x", 129), "..",
	}
	for _, name := range invalid {
		if _, err := SanitizeName(name); !errors.Is(err, ErrInvalidName) {
			t.Errorf("%q should be invalid, got %v", name, err)
		}
	}
}

func TestStoreRejectsBadNameAndOversize(t *testing.T) {
	store := NewStore(filepath.Join(t.TempDir(), "assets"))

	if _, err := store.Save("../evil.png", []byte("x")); !errors.Is(err, ErrInvalidName) {
		t.Errorf("path traversal should be rejected, got %v", err)
	}
	big := make([]byte, MaxAssetSize+1)
	if _, err := store.Save("big.png", big); !errors.Is(err, ErrAssetTooLarge) {
		t.Errorf("oversize should be rejected, got %v", err)
	}
	// 恰好在上限内可存
	if _, err := store.Save("edge.png", make([]byte, MaxAssetSize)); err != nil {
		t.Errorf("max-size asset should save, got %v", err)
	}
}

func TestStoreConcurrentSaveSameName(t *testing.T) {
	store := NewStore(filepath.Join(t.TempDir(), "assets"))
	var wg sync.WaitGroup
	for i := 0; i < 16; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			if _, err := store.Save("race.png", []byte{byte(i)}); err != nil {
				t.Errorf("concurrent save %d: %v", i, err)
			}
		}(i)
	}
	wg.Wait()
	// 最终状态：内容文件与 sidecar 均存在且可读
	if _, _, err := store.Get("race.png"); err != nil {
		t.Errorf("get after concurrent saves: %v", err)
	}
}

func TestStoreGetDirectoryEscapeRejected(t *testing.T) {
	store := NewStore(filepath.Join(t.TempDir(), "assets"))
	if err := os.MkdirAll(filepath.Join(store.Dir(), "sub"), 0o755); err != nil {
		t.Fatal(err)
	}
	if _, _, err := store.Get("../" + filepath.Base(store.Dir()) + "/sub/x"); !errors.Is(err, ErrInvalidName) {
		t.Errorf("escape via get should be rejected, got %v", err)
	}
}
