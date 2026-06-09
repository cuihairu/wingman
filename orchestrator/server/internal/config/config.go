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
)

type Config struct {
	Host        string
	Port        int
	DBPath      string
	StaticDir   string
	AgentAddr   string
	ScriptsDir  string
	JWTSecret   string
	CORSOrigins []string
}

func Load() (Config, error) {
	cfg := Config{
		Host:        getenv("WINGMAN_HOST", defaultHost),
		Port:        getenvInt("WINGMAN_PORT", defaultPort),
		DBPath:      getenv("WINGMAN_DB_PATH", defaultDBPath),
		StaticDir:   getenv("WINGMAN_STATIC_DIR", defaultStaticDir),
		AgentAddr:   getenv("WINGMAN_AGENT_ADDR", defaultAgentAddr),
		ScriptsDir:  getenv("WINGMAN_SCRIPTS_DIR", defaultScriptsDir),
		JWTSecret:   os.Getenv("WINGMAN_JWT_SECRET"),
		CORSOrigins: splitList(os.Getenv("WINGMAN_CORS_ORIGINS")),
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

	absScriptsDir, err := filepath.Abs(cfg.ScriptsDir)
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
