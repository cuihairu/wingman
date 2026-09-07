package scripts

import (
	"fmt"
	"path/filepath"
	"strings"
)

type Store struct {
	root string
}

// filepathRel 以变量形式间接引用 filepath.Rel，便于测试注入失败场景。
var filepathRel = filepath.Rel

func NewStore(root string) Store {
	return Store{root: filepath.Clean(root)}
}

func (s Store) Root() string {
	return s.root
}

func (s Store) Resolve(nameOrPath string) (string, error) {
	if strings.TrimSpace(nameOrPath) == "" {
		return "", fmt.Errorf("script path is required")
	}

	cleaned := filepath.Clean(nameOrPath)
	if filepath.IsAbs(cleaned) {
		return "", fmt.Errorf("absolute script paths are not allowed")
	}

	fullPath := filepath.Join(s.root, cleaned)
	fullPath = filepath.Clean(fullPath)

	rel, err := filepathRel(s.root, fullPath)
	if err != nil {
		return "", err
	}
	if rel == "." || strings.HasPrefix(rel, ".."+string(filepath.Separator)) || rel == ".." {
		return "", fmt.Errorf("script path escapes scripts directory")
	}
	if filepath.Ext(fullPath) != ".lua" {
		return "", fmt.Errorf("only .lua scripts are allowed")
	}

	return fullPath, nil
}

func DisplayPath(root, fullPath string) string {
	rel, err := filepath.Rel(root, fullPath)
	if err != nil {
		return fullPath
	}
	return filepath.ToSlash(rel)
}
