package handlers

// VaultProvider 切换验收（根 todo.md 验收①：只改配置换实现，不改任何业务
// 代码，原功能语义由新实现接管）。stub 语义故意与 local 全不同（unlock
// 永远成功、status 恒解锁、lookup 出固定凭据），同一批 handler/同一调用
// 方在切换前后行为跟着 provider 走，即为「业务面零改动可切换」的实测留证。

import (
	"net/http"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/vault"
)

const stubName = "test-stub"

// stubProvider 内存假实现（无落库、无加密）。
type stubProvider struct{}

func (stubProvider) Name() string { return stubName }
func (stubProvider) Status(uint) (vault.MasterStatus, error) {
	return vault.MasterStatus{Configured: true, Unlocked: true, UnlockedUntil: "2099-01-01T00:00:00Z", AutoLockAfterS: 60}, nil
}
func (stubProvider) Setup(uint, string) (time.Time, error) {
	return time.Now().Add(time.Minute), nil
}
func (stubProvider) Unlock(uint, string) (time.Time, error) {
	return time.Now().Add(time.Minute), nil
}
func (stubProvider) ChangePassword(uint, string, string) error { return nil }
func (stubProvider) Lock(uint) bool                            { return true }
func (stubProvider) List(uint) ([]vault.EntryMeta, error)      { return []vault.EntryMeta{}, nil }
func (stubProvider) Upsert(uint, vault.Entry) (vault.EntryMeta, bool, error) {
	return vault.EntryMeta{AgentID: "stub-entry"}, true, nil
}
func (stubProvider) Delete(uint, uint) error { return nil }
func (stubProvider) Export(uint) (map[string]any, error) {
	return map[string]any{"format": "stub-v1", "credentials": []any{}}, nil
}
func (stubProvider) Lookup(uint, string, string, int) (string, string, string, error) {
	return "stub-user", "stub-pass", "", nil
}

func TestVaultProviderSwitchByConfig(t *testing.T) {
	db := newDB(t)
	vault.Register(stubName, func(vault.Options) (vault.Provider, error) { return stubProvider{}, nil })
	vaultActivate(t, db, nil)
	h := NewVaultHandler(db)
	const uid = uint(109)

	// local 语义基线：未配置时 unlock 409
	if code, _ := vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"whatever1"}`); code != http.StatusConflict {
		t.Fatalf("local unlock before setup should 409, got %d", code)
	}

	// 切换 = 换激活（生产里由 WINGMAN_VAULT_PROVIDER 驱动），业务代码零改动：
	// stub 语义即时接管同一批端点
	if err := vault.Activate(stubName, vault.Options{}); err != nil {
		t.Fatalf("activate stub: %v", err)
	}
	if name := vault.ActiveName(); name != stubName {
		t.Fatalf("active should be %q, got %q", stubName, name)
	}
	if code, resp := vaultDo(t, h, uid, "GET", "/status", ""); code != 200 ||
		resp["data"].(map[string]any)["unlocked"] != true {
		t.Fatalf("stub status should be unlocked: %d %v", code, resp)
	}
	if code, _ := vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"whatever1"}`); code != 200 {
		t.Fatalf("stub unlock should accept any password, got %d", code)
	}

	// 适配层职责不随 provider 走：形状校验仍在本层（坏协议 400）、审计照记
	if code, _ := vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"a1","protocol":"ftp","password":"x"}`); code != http.StatusBadRequest {
		t.Fatalf("handler-side validation must survive switch, got %d", code)
	}

	// 票据取用同走激活 provider：同一调用方拿到 stub 凭据
	user, pass, _, err := VaultLookupSaved(uid, "ag1", "ssh", 22)
	if err != nil || user != "stub-user" || pass != "stub-pass" {
		t.Fatalf("stub lookup: %q %q %v", user, pass, err)
	}

	// 切回 local：语义复原（stub 不落库，local 视角回到未配置）
	vaultActivate(t, db, nil)
	if code, resp := vaultDo(t, h, uid, "GET", "/status", ""); code != 200 ||
		resp["data"].(map[string]any)["configured"] != false {
		t.Fatalf("back to local should be unconfigured: %d %v", code, resp)
	}
	if code, _ := vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"whatever1"}`); code != http.StatusConflict {
		t.Fatalf("local semantics restored should 409, got %d", code)
	}
}

func TestVaultProviderNotConfiguredReports500(t *testing.T) {
	// 未装配 provider（启动装配缺位的兜底语义）：显式报未配置，不悄悄回退
	h := &VaultHandler{db: newDB(t), pick: func() vault.Provider { return nil }}

	code, resp := vaultDo(t, h, 110, "GET", "/status", "")
	if code != http.StatusInternalServerError || resp["error"] != "vault provider not configured" {
		t.Fatalf("unconfigured provider should 500: %d %v", code, resp)
	}
}
