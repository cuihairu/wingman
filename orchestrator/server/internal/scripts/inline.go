package scripts

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// MaxInlineScriptSize run_script 内联下发脚本内容的大小上限。
// 帧协议上限 16MB，这里留足余量；超大脚本应拆分或走 asset 分发（A4）。
const MaxInlineScriptSize = 1 << 20

// ErrScriptTooLarge 脚本内容超过 MaxInlineScriptSize。
var ErrScriptTooLarge = errors.New("script too large")

// ReadInline 读取脚本内容供内联下发（Android 等无服务器文件系统的 agent
// 依赖 content 字段；桌面 runtime 忽略 content 继续用 path，向后兼容）。
// 见 docs/android-agent-design.md §3.2。超过上限返回 ErrScriptTooLarge。
func ReadInline(path string) ([]byte, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	if len(content) > MaxInlineScriptSize {
		return nil, fmt.Errorf("%w: %d bytes (max %d)",
			ErrScriptTooLarge, len(content), MaxInlineScriptSize)
	}
	return content, nil
}

// LanguageOf 按扩展名推断脚本语言（Android 端 A1 仅执行 lua；
// 未知扩展名返回空串，由 agent 侧自行判定）。
func LanguageOf(path string) string {
	switch strings.ToLower(filepath.Ext(path)) {
	case ".lua":
		return "lua"
	case ".py":
		return "python"
	default:
		return ""
	}
}
