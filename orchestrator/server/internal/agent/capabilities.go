package agent

// Capability System（ADR: Capability System）：
// - 服务端持有统一能力词汇表；agent.register 上报的能力集经 Registry
//   内存持有并经 CapabilityStore 持久化（与 Tags 同构，Registry 不依赖 gorm）。
// - 上报的未知能力仍然存储与展示（registry UI 可标记 unverified），
//   仅当 workflow 步骤声明 requires 时才做词汇内的匹配判断。
// - 新增能力词汇需在本表登记后再被 requires 使用。

// KnownCapabilities 服务端认可的能力词汇表。
var KnownCapabilities = map[string]bool{
	"screen.capture":      true,
	"screen.stream":       true,
	"screen.listMonitors": true,
	"input.mouse":         true,
	"input.keyboard":      true,
	"input.touch":         true,
	"window.enumerate":    true,
	"window.activate":     true,
	"process.spawn":       true,
	"vision.image":        true,
	"vision.color":        true,
	"ocr":                 true,
	"ml.onnx":             true,
}

// CapabilityStore 能力集持久化接口（由 DB 层实现，见 handlers.NewAgentCapabilityStore）。
// 与 TagStore 同构：Registry 只依赖接口，DB IO 全部在锁外执行。
type CapabilityStore interface {
	// LoadCapabilities 返回持久化的能力集；(nil, false) 表示无记录或解析失败。
	LoadCapabilities(agentID string) ([]string, bool)
	// SaveCapabilities 持久化能力集（无记录则建行，有则更新）。
	SaveCapabilities(agentID string, caps []string) error
}
