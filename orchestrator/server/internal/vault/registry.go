package vault

import (
	"fmt"
	"sort"
	"sync"

	"gorm.io/gorm"
)

// Factory provider 构造器（Activate 时以配置实例化）。
type Factory func(Options) (Provider, error)

// Options provider 构造参数：
//   - DB server 主库（local 实现落库用；外部 provider 可忽略）；
//   - Config 该 provider 的配置节（WINGMAN_VAULT_<NAME>_* 环境变量去公共
//     前缀后的大写键值，如 WINGMAN_VAULT_LOCAL_SESSION_TTL →
//     LOCAL_SESSION_TTL=30m；解析与校验由实现负责，非法值启动即炸）。
type Options struct {
	DB     *gorm.DB
	Config map[string]string
}

var (
	regMu    sync.RWMutex
	registry = map[string]Factory{}

	actMu   sync.RWMutex
	active  Provider
	actName string
)

// Register 注册一个 provider 实现（同名覆盖，后注册者生效；init 期或测试
// 装配用）。空名/空工厂直接 panic——注册面是编译期装配，不该有运行时分支。
func Register(name string, factory Factory) {
	if name == "" || factory == nil {
		panic("vault: bad provider registration")
	}
	regMu.Lock()
	defer regMu.Unlock()
	registry[name] = factory
}

// Activate 按配置名实例化并激活 provider（进程级单选）。未知名/构造失败
// 返回错误（启动装配处应 fatal——配置错误不静默降级）。
func Activate(name string, opts Options) error {
	regMu.RLock()
	factory := registry[name]
	regMu.RUnlock()
	if factory == nil {
		return fmt.Errorf("vault: unknown provider %q (registered: %v)", name, Names())
	}
	p, err := factory(opts)
	if err != nil {
		return fmt.Errorf("vault: init provider %q: %w", name, err)
	}
	actMu.Lock()
	active = p
	actName = name
	actMu.Unlock()
	return nil
}

// Active 当前激活的 provider；未激活返回 nil（启动未装配或测试未接线）——
// 调用方面向 nil 显式报错，不悄悄回退到任何默认实现。
func Active() Provider {
	actMu.RLock()
	defer actMu.RUnlock()
	return active
}

// ActiveName 当前激活名（未激活为空串）。
func ActiveName() string {
	actMu.RLock()
	defer actMu.RUnlock()
	return actName
}

// Names 已注册 provider 名（排序；启动报错与配置文档用）。
func Names() []string {
	regMu.RLock()
	defer regMu.RUnlock()
	names := make([]string, 0, len(registry))
	for name := range registry {
		names = append(names, name)
	}
	sort.Strings(names)
	return names
}
