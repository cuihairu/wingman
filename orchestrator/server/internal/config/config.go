package config

import (
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const (
	defaultPort       = 9527
	defaultHost       = "127.0.0.1"
	defaultDBPath     = "./data/wingman.db"
	defaultStaticDir  = "../build/dist"
	defaultAgentAddr  = "127.0.0.1:8888"
	defaultScriptsDir = "./scripts"
	// 自动更新制品根目录（WINGMAN_UPDATES_DIR）；admin 发布的安装包落
	// <UpdatesDir>/<channel>/<platform>-<arch>/，DB 只存元数据（models.UpdateRelease）。
	defaultUpdatesDir = "./data/updates"
	// 资产（模板图片等脚本配套文件）服务端存储根目录（WINGMAN_ASSETS_DIR）；
	// 内容落 <AssetsDir>/<name>，元数据 sidecar 落 <AssetsDir>/.meta/。
	defaultAssetsDir = "./data/assets"
	defaultGuacdAddr = "127.0.0.1:4822"
	// 默认虚拟盘路径与 deployments/guacd/docker-compose.yml 的挂卷对齐
	defaultGuacdDrivePath = "/wingman-drive"
)

type Config struct {
	Host       string
	Port       int
	DBPath     string
	StaticDir  string
	AgentAddr  string
	ScriptsDir string
	// UpdatesDir 自动更新制品根目录（见 defaultUpdatesDir）。
	UpdatesDir string
	// AssetsDir 资产服务端存储根目录（见 defaultAssetsDir）。
	AssetsDir   string
	JWTSecret   string
	CORSOrigins []string
	// AgentTokens agent 注册 token 列表（WINGMAN_AGENT_TOKENS，逗号分隔）。
	// 空 = 关闭 agent 注册鉴权（默认，向后兼容既有部署）。
	// 设计见 docs/agent-token-auth-design.md。
	AgentTokens []string
	// GuacdAddr guacd 协议翻译守护进程地址（WINGMAN_GUACD_ADDR）。
	// 像素面（Guacamole 远程桌面）反代目标；仅同机回环/内网，不经公网。
	// 设计见 docs/remote-gateway-guacamole-design.md §4/DG-6。
	GuacdAddr string
	// GuacdDrivePath RDP 文件传输虚拟盘在 guacd 容器内的路径
	// （WINGMAN_GUACD_DRIVE_PATH）。设计 §15：RDP 走设备重定向虚拟盘，
	// drive-path 挂载在 guacd 侧卷上，部署需对应挂卷。
	GuacdDrivePath string
	// GuacdRecordingPath 会话录制写入目录的 guacd 侧路径
	// （WINGMAN_GUACD_RECORDING_PATH）。注入 connect 参数 recording-path。
	GuacdRecordingPath string
	// RecordingDir 会话录像目录的 Go server 侧路径（WINGMAN_RECORDING_DIR）。
	// 与 GuacdRecordingPath 指向同一宿主卷的两个挂载点（可不同容器路径）。
	// 两者均非空才允许 record=true 的票据与录像检索 API；空 = 录制关闭。
	RecordingDir string
	// VaultProvider 密钥保险箱 provider（WINGMAN_VAULT_PROVIDER，默认
	// local = 自托管本地加密实现）。可选值见 internal/vault 注册表；未知值
	// 在启动装配 vault.Activate 时报错退出，不静默降级。
	VaultProvider string
	// VaultProviderConfig 各 provider 的配置节：WINGMAN_VAULT_<NAME>_* 环境
	// 变量去公共前缀后的大写键值（如 WINGMAN_VAULT_LOCAL_SESSION_TTL →
	// LOCAL_SESSION_TTL）。值原样透传，由 provider 自行解析校验。
	VaultProviderConfig map[string]string
}

// filepathAbs 以变量形式间接引用 filepath.Abs，便于测试注入失败场景
// （cwd 不可用等平台相关行为无法跨平台稳定复现）。
var filepathAbs = filepath.Abs

func Load() (Config, error) {
	cfg := Config{
		Host:                getenv("WINGMAN_HOST", defaultHost),
		Port:                getenvInt("WINGMAN_PORT", defaultPort),
		DBPath:              getenv("WINGMAN_DB_PATH", defaultDBPath),
		StaticDir:           getenv("WINGMAN_STATIC_DIR", defaultStaticDir),
		AgentAddr:           getenv("WINGMAN_AGENT_ADDR", defaultAgentAddr),
		ScriptsDir:          getenv("WINGMAN_SCRIPTS_DIR", defaultScriptsDir),
		UpdatesDir:          getenv("WINGMAN_UPDATES_DIR", defaultUpdatesDir),
		AssetsDir:           getenv("WINGMAN_ASSETS_DIR", defaultAssetsDir),
		JWTSecret:           os.Getenv("WINGMAN_JWT_SECRET"),
		CORSOrigins:         splitList(os.Getenv("WINGMAN_CORS_ORIGINS")),
		AgentTokens:         splitList(os.Getenv("WINGMAN_AGENT_TOKENS")),
		GuacdAddr:           getenv("WINGMAN_GUACD_ADDR", defaultGuacdAddr),
		GuacdDrivePath:      getenv("WINGMAN_GUACD_DRIVE_PATH", defaultGuacdDrivePath),
		GuacdRecordingPath:  getenv("WINGMAN_GUACD_RECORDING_PATH", ""),
		RecordingDir:        getenv("WINGMAN_RECORDING_DIR", ""),
		VaultProvider:       getenv("WINGMAN_VAULT_PROVIDER", "local"),
		VaultProviderConfig: vaultProviderConfig(),
	}

	if cfg.JWTSecret == "" {
		return cfg, fmt.Errorf("WINGMAN_JWT_SECRET is required")
	}
	if len(cfg.JWTSecret) < 32 {
		return cfg, fmt.Errorf("WINGMAN_JWT_SECRET must be at least 32 characters")
	}
	if net.ParseIP(cfg.Host) == nil && cfg.Host != "localhost" {
		return cfg, fmt.Errorf("invalid WINGMAN_HOST: %s", cfg.Host)
	}
	if cfg.Port <= 0 || cfg.Port > 65535 {
		return cfg, fmt.Errorf("invalid WINGMAN_PORT: %d", cfg.Port)
	}

	absScriptsDir, err := filepathAbs(cfg.ScriptsDir)
	if err != nil {
		return cfg, err
	}
	cfg.ScriptsDir = filepath.Clean(absScriptsDir)

	return cfg, nil
}

func MustLoad() Config {
	cfg, err := Load()
	if err != nil {
		panic(err)
	}
	return cfg
}

func Addr(cfg Config) string {
	return fmt.Sprintf("%s:%d", cfg.Host, cfg.Port)
}

// vaultProviderConfig 收集 provider 配置节：环境变量 WINGMAN_VAULT_<NAME>_
// * 去公共前缀后的大写键值（如 WINGMAN_VAULT_LOCAL_SESSION_TTL →
// LOCAL_SESSION_TTL）；选择项 WINGMAN_VAULT_PROVIDER 本身除外。值原样
// 透传，解析与校验由 provider 实现负责。
func vaultProviderConfig() map[string]string {
	const prefix = "WINGMAN_VAULT_"
	out := map[string]string{}
	for _, kv := range os.Environ() {
		key, value, found := strings.Cut(kv, "=")
		if !found || !strings.HasPrefix(key, prefix) || key == "WINGMAN_VAULT_PROVIDER" {
			continue
		}
		out[key[len(prefix):]] = value
	}
	return out
}

func getenv(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

func getenvInt(key string, fallback int) int {
	value := os.Getenv(key)
	if value == "" {
		return fallback
	}
	parsed, err := strconv.Atoi(value)
	if err != nil {
		return fallback
	}
	return parsed
}

func splitList(value string) []string {
	if value == "" {
		return nil
	}
	parts := strings.Split(value, ",")
	result := make([]string, 0, len(parts))
	for _, part := range parts {
		trimmed := strings.TrimSpace(part)
		if trimmed != "" {
			result = append(result, trimmed)
		}
	}
	return result
}
