// Package assets 提供 Go server 侧的模板图片资产存储。
//
// 资产（模板图片等脚本配套文件）经 Dashboard/HTTP API 上传落盘于
// <AssetsDir>/<name>，元数据（sha256/大小/时间）落 <AssetsDir>/.meta/<name>.json
// sidecar；下发走 agent 通道 asset.sync 命令（handlers.AssetHandler，
// Dispatcher Reuse，见 docs/architecture-decisions.md「Agent Groups &
// Batch Operations」）。
//
// 上限与 internal/scripts.MaxInlineScriptSize 同口径（1MB）：帧协议上限
// 16MB，内联 base64 后仍留足余量；超大资产应走 URL 拉取分发（A4 演进，
// 当前不启用）。
package assets

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

// MaxAssetSize 单资产内容上限（字节）。
const MaxAssetSize = 1 << 20

var (
	// ErrAssetTooLarge 资产内容超过 MaxAssetSize。
	ErrAssetTooLarge = errors.New("asset too large")
	// ErrAssetNotFound 资产不存在。
	ErrAssetNotFound = errors.New("asset not found")
	// ErrInvalidName 资产名非法（空/路径穿越/控制字符等）。
	ErrInvalidName = errors.New("invalid asset name")
)

// namePattern 资产名白名单：字母数字与 . _ -（不含路径分隔符，防穿越）。
var namePattern = regexp.MustCompile(`^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$`)

// SanitizeName 校验并返回资产名（同时用于上传与下发 payload 校验，
// 两端规则一致）。非法名返回 ErrInvalidName。
func SanitizeName(name string) (string, error) {
	if !namePattern.MatchString(name) || strings.Contains(name, "..") {
		return "", fmt.Errorf("%w: %q", ErrInvalidName, name)
	}
	return name, nil
}

// Meta 资产元数据（sidecar JSON 与 API 返回共用）。
type Meta struct {
	Name       string    `json:"name"`
	Size       int64     `json:"size"`
	Sha256     string    `json:"sha256"`
	UploadedAt time.Time `json:"uploadedAt"`
}

// Store 资产存储（目录内元数据 sidecar + 内容文件，无 DB 依赖）。
// 并发安全（Save/List/Get/Delete 互斥）。
type Store struct {
	mu  sync.Mutex
	dir string
}

// NewStore 在 dir 下初始化资产存储（目录惰性创建，Save 时落盘）。
func NewStore(dir string) *Store {
	return &Store{dir: dir}
}

// Dir 返回存储根目录。
func (s *Store) Dir() string { return s.dir }

func (s *Store) metaDir() string { return filepath.Join(s.dir, ".meta") }

func (s *Store) contentPath(name string) string {
	return filepath.Join(s.dir, name)
}

func (s *Store) metaPath(name string) string {
	return filepath.Join(s.metaDir(), name+".json")
}

// Save 写入资产内容（原子：临时文件 + rename），计算 sha256 并落元数据
// sidecar。同名覆盖（sha256 随内容更新，即隐式版本号）。
func (s *Store) Save(name string, content []byte) (Meta, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, err := SanitizeName(name); err != nil {
		return Meta{}, err
	}
	if len(content) > MaxAssetSize {
		return Meta{}, fmt.Errorf("%w: %d bytes (max %d)", ErrAssetTooLarge, len(content), MaxAssetSize)
	}
	if err := os.MkdirAll(s.dir, 0o755); err != nil {
		return Meta{}, err
	}
	if err := os.MkdirAll(s.metaDir(), 0o755); err != nil {
		return Meta{}, err
	}

	sum := sha256.Sum256(content)
	meta := Meta{
		Name:       name,
		Size:       int64(len(content)),
		Sha256:     hex.EncodeToString(sum[:]),
		UploadedAt: time.Now().UTC(),
	}

	tmp, err := os.CreateTemp(s.dir, ".upload-*")
	if err != nil {
		return Meta{}, err
	}
	tmpPath := tmp.Name()
	if _, err := tmp.Write(content); err != nil {
		tmp.Close()
		os.Remove(tmpPath)
		return Meta{}, err
	}
	if err := tmp.Close(); err != nil {
		os.Remove(tmpPath)
		return Meta{}, err
	}
	if err := os.Rename(tmpPath, s.contentPath(name)); err != nil {
		os.Remove(tmpPath)
		return Meta{}, err
	}

	metaBytes, err := json.Marshal(meta)
	if err != nil {
		return meta, err
	}
	if err := os.WriteFile(s.metaPath(name), metaBytes, 0o644); err != nil {
		return meta, err
	}
	return meta, nil
}

// List 返回全部资产元数据（按名称排序；sidecar 缺失/损坏的条目跳过）。
func (s *Store) List() ([]Meta, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	entries, err := os.ReadDir(s.metaDir())
	if os.IsNotExist(err) {
		return []Meta{}, nil
	}
	if err != nil {
		return nil, err
	}
	out := []Meta{}
	for _, entry := range entries {
		if entry.IsDir() || !strings.HasSuffix(entry.Name(), ".json") {
			continue
		}
		raw, err := os.ReadFile(filepath.Join(s.metaDir(), entry.Name()))
		if err != nil {
			continue
		}
		var meta Meta
		if json.Unmarshal(raw, &meta) != nil || meta.Name == "" {
			continue
		}
		out = append(out, meta)
	}
	return out, nil
}

// Get 返回资产内容与元数据；不存在返回 ErrAssetNotFound。
func (s *Store) Get(name string) ([]byte, Meta, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, err := SanitizeName(name); err != nil {
		return nil, Meta{}, err
	}
	meta, err := s.readMeta(name)
	if err != nil {
		return nil, Meta{}, err
	}
	content, err := os.ReadFile(s.contentPath(name))
	if os.IsNotExist(err) {
		return nil, Meta{}, ErrAssetNotFound
	}
	if err != nil {
		return nil, Meta{}, err
	}
	return content, meta, nil
}

// Delete 移除资产内容与元数据 sidecar；不存在返回 ErrAssetNotFound。
func (s *Store) Delete(name string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, err := SanitizeName(name); err != nil {
		return err
	}
	if _, err := s.readMeta(name); err != nil {
		return err
	}
	if err := os.Remove(s.contentPath(name)); err != nil && !os.IsNotExist(err) {
		return err
	}
	if err := os.Remove(s.metaPath(name)); err != nil && !os.IsNotExist(err) {
		return err
	}
	return nil
}

func (s *Store) readMeta(name string) (Meta, error) {
	raw, err := os.ReadFile(s.metaPath(name))
	if os.IsNotExist(err) {
		return Meta{}, ErrAssetNotFound
	}
	if err != nil {
		return Meta{}, err
	}
	var meta Meta
	if json.Unmarshal(raw, &meta) != nil {
		return Meta{}, ErrAssetNotFound
	}
	return meta, nil
}
