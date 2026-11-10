package vault

import (
	"encoding/json"
	"errors"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// LocalProvider 全生命周期行为面：主口令状态机（setup/unlock/change/lock）、
// 凭据条目 CRUD、导出与连接取用。哨兵错误逐个钉住——HTTP 层按它们映射
// 状态码，语义漂移即测试红。

func newLocalProv(t *testing.T) *LocalProvider {
	t.Helper()
	f, err := NewLocal(Options{DB: newTestDB(t)})
	if err != nil {
		t.Fatalf("new local: %v", err)
	}
	if f.Name() != LocalName {
		t.Fatalf("local provider name = %q, want %q", f.Name(), LocalName)
	}
	return f.(*LocalProvider)
}

func TestLocalMasterLifecycle(t *testing.T) {
	p := newLocalProv(t)
	const uid uint = 11
	const pass = "master-pass-1"

	// 未配置阶段：Unlock / ChangePassword 都按「未配置」拒绝
	if _, err := p.Unlock(uid, pass); !errors.Is(err, ErrNotConfigured) {
		t.Fatalf("unlock before setup = %v, want ErrNotConfigured", err)
	}
	if err := p.ChangePassword(uid, pass, "new-pass-99"); !errors.Is(err, ErrNotConfigured) {
		t.Fatalf("change before setup = %v, want ErrNotConfigured", err)
	}

	// 弱口令在配置与换口令两条路上都拦
	if _, err := p.Setup(uid, "short"); !errors.Is(err, ErrWeakMaster) {
		t.Fatalf("weak setup = %v, want ErrWeakMaster", err)
	}
	until, err := p.Setup(uid, pass)
	if err != nil {
		t.Fatalf("setup: %v", err)
	}
	if !until.After(time.Now()) {
		t.Fatalf("setup should leave session unlocked until %v", until)
	}
	if _, err := p.Setup(uid, pass); !errors.Is(err, ErrAlreadyConfigured) {
		t.Fatalf("re-setup = %v, want ErrAlreadyConfigured", err)
	}

	st, err := p.Status(uid)
	if err != nil || !st.Configured || !st.Unlocked {
		t.Fatalf("after setup status should be configured+unlocked: %+v %v", st, err)
	}
	if _, err := time.Parse(time.RFC3339, st.UnlockedUntil); err != nil {
		t.Fatalf("unlockedUntil should be RFC3339: %q %v", st.UnlockedUntil, err)
	}

	// 错口令解锁与错当前口令换口令：各自哨兵错误（GCM 认证失败统一折叠）
	if _, err := p.Unlock(uid, "wrong-pass-x"); !errors.Is(err, ErrWrongMaster) {
		t.Fatalf("wrong unlock = %v, want ErrWrongMaster", err)
	}
	if err := p.ChangePassword(uid, "wrong-pass-x", "new-pass-99"); !errors.Is(err, ErrWrongCurrent) {
		t.Fatalf("wrong current = %v, want ErrWrongCurrent", err)
	}
	if err := p.ChangePassword(uid, pass, "short"); !errors.Is(err, ErrWeakMaster) {
		t.Fatalf("weak new = %v, want ErrWeakMaster", err)
	}

	// 换口令成功：旧口令立即失效、新口令解锁
	if err := p.ChangePassword(uid, pass, "new-pass-99"); err != nil {
		t.Fatalf("change password: %v", err)
	}
	if _, err := p.Unlock(uid, pass); !errors.Is(err, ErrWrongMaster) {
		t.Fatalf("old pass after change = %v, want ErrWrongMaster", err)
	}
	if _, err := p.Unlock(uid, "new-pass-99"); err != nil {
		t.Fatalf("unlock with new pass: %v", err)
	}

	// Lock 幂等：解锁态删一次 true，再删 false
	if !p.Lock(uid) {
		t.Fatal("lock on unlocked should report was-unlocked")
	}
	if p.Lock(uid) {
		t.Fatal("second lock should report not-unlocked")
	}
	st, err = p.Status(uid)
	if err != nil || !st.Configured || st.Unlocked || st.UnlockedUntil != "" {
		t.Fatalf("locked status: %+v %v", st, err)
	}
}

func TestLocalChangePasswordKeepsCredentialsDecryptable(t *testing.T) {
	p := newLocalProv(t)
	const uid uint = 12
	if _, err := p.Setup(uid, "master-pass-1"); err != nil {
		t.Fatalf("setup: %v", err)
	}
	if _, _, err := p.Upsert(uid, Entry{AgentID: "a1", Protocol: "ssh", Port: 22, Password: "sekret-pass"}); err != nil {
		t.Fatalf("upsert: %v", err)
	}
	// 换口令只重包裹 DEK：已存密文对新口令继续有效
	if err := p.ChangePassword(uid, "master-pass-1", "new-pass-99"); err != nil {
		t.Fatalf("change: %v", err)
	}
	p.Lock(uid)
	if _, _, err := p.Upsert(uid, Entry{AgentID: "a1", Protocol: "ssh", Port: 22}); !errors.Is(err, ErrLocked) {
		t.Fatalf("locked upsert = %v, want ErrLocked", err)
	}
	if err := p.Delete(uid, 1); !errors.Is(err, ErrLocked) {
		t.Fatalf("locked delete = %v, want ErrLocked", err)
	}
	if _, _, _, err := p.Lookup(uid, "a1", "ssh", 0); !errors.Is(err, ErrLocked) {
		t.Fatalf("locked lookup = %v, want ErrLocked", err)
	}
	if _, err := p.Unlock(uid, "new-pass-99"); err != nil {
		t.Fatalf("unlock with new pass: %v", err)
	}
	_, pw, _, err := p.Lookup(uid, "a1", "ssh", 0)
	if err != nil {
		t.Fatalf("lookup after change: %v", err)
	}
	if pw != "sekret-pass" {
		t.Fatalf("credential must stay decryptable across change, got %q", pw)
	}
}

func TestLocalUpsertSemantics(t *testing.T) {
	p := newLocalProv(t)
	const uid uint = 13
	if _, err := p.Setup(uid, "master-pass-1"); err != nil {
		t.Fatalf("setup: %v", err)
	}

	// 新建条目不给任何秘密字段 → 拒绝
	if _, _, err := p.Upsert(uid, Entry{AgentID: "a1", Protocol: "rdp", Port: 3389, Label: "L"}); !errors.Is(err, ErrNeedSecret) {
		t.Fatalf("new entry without secret = %v, want ErrNeedSecret", err)
	}

	meta, created, err := p.Upsert(uid, Entry{
		AgentID: "a1", Protocol: "rdp", Port: 3389,
		Label: "work rdp", Username: "admin", Domain: "CORP",
		Password: "pw-one",
	})
	if err != nil || !created {
		t.Fatalf("first upsert: created=%v err=%v", created, err)
	}
	if meta.ID == 0 || !meta.HasPassword || !meta.HasSecret || meta.Label != "work rdp" ||
		meta.Username != "admin" || meta.Domain != "CORP" {
		t.Fatalf("meta round-trip: %+v", meta)
	}

	// 同元组再存 = 更新；秘密留空保留旧值（「改标签不重输密码」路径）
	meta2, created2, err := p.Upsert(uid, Entry{
		AgentID: "a1", Protocol: "rdp", Port: 3389, Label: "renamed", Username: "admin2",
	})
	if err != nil || created2 {
		t.Fatalf("second upsert: created=%v err=%v", created2, err)
	}
	if meta2.ID != meta.ID || meta2.Label != "renamed" || !meta2.HasPassword {
		t.Fatalf("update should keep id and password: %+v", meta2)
	}
	if _, pw, _, err := p.Lookup(uid, "a1", "rdp", 3389); err != nil || pw != "pw-one" {
		t.Fatalf("empty-secret update must keep old password, got %q %v", pw, err)
	}

	// 显式给新密码 = 替换
	if _, _, err := p.Upsert(uid, Entry{AgentID: "a1", Protocol: "rdp", Port: 3389, Password: "pw-two"}); err != nil {
		t.Fatalf("replace password: %v", err)
	}
	if _, pw, _, err := p.Lookup(uid, "a1", "rdp", 3389); err != nil || pw != "pw-two" {
		t.Fatalf("explicit password must replace, got %q %v", pw, err)
	}

	// 只带私钥的新条目：HasPassword false、HasSecret true，密码位为空
	meta3, created3, err := p.Upsert(uid, Entry{AgentID: "a2", Protocol: "ssh", Port: 22, PrivateKey: "key-material"})
	if err != nil || !created3 || meta3.HasPassword || !meta3.HasSecret {
		t.Fatalf("key-only entry: created=%v meta=%+v err=%v", created3, meta3, err)
	}
	if _, pw, _, err := p.Lookup(uid, "a2", "ssh", 0); err != nil || pw != "" {
		t.Fatalf("key-only lookup should have empty password, got %q %v", pw, err)
	}

	list, err := p.List(uid)
	if err != nil || len(list) != 2 {
		t.Fatalf("list: n=%d err=%v", len(list), err)
	}
}

func TestLocalDeleteOwnershipAndMissing(t *testing.T) {
	p := newLocalProv(t)
	if _, err := p.Setup(21, "master-pass-1"); err != nil {
		t.Fatalf("setup: %v", err)
	}
	meta, _, err := p.Upsert(21, Entry{AgentID: "a1", Protocol: "ssh", Port: 22, Password: "pw"})
	if err != nil {
		t.Fatalf("upsert: %v", err)
	}

	if err := p.Delete(21, meta.ID+999); !errors.Is(err, ErrNotFound) {
		t.Fatalf("delete missing = %v, want ErrNotFound", err)
	}
	// 未配置用户的会话本来就不存在 → ErrLocked；非本人条目按不存在处理
	if _, err := p.Setup(22, "master-pass-1"); err != nil {
		t.Fatalf("setup user22: %v", err)
	}
	if _, _, err := p.Upsert(22, Entry{AgentID: "a2", Protocol: "vnc", Port: 5900, Password: "pw2"}); err != nil {
		t.Fatalf("upsert user22: %v", err)
	}
	if err := p.Delete(22, meta.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("cross-user delete = %v, want ErrNotFound", err)
	}
	if err := p.Delete(21, meta.ID); err != nil {
		t.Fatalf("own delete: %v", err)
	}
	if list, err := p.List(21); err != nil || len(list) != 0 {
		t.Fatalf("list after delete: n=%d err=%v", len(list), err)
	}
}

func TestLocalExportShape(t *testing.T) {
	p := newLocalProv(t)
	const uid uint = 31
	const pass = "master-pass-1"

	// 无会话 → 先按锁定拒绝；有会话无主记录（白盒构造）→ 未配置
	if _, err := p.Export(uid); !errors.Is(err, ErrLocked) {
		t.Fatalf("locked export = %v, want ErrLocked", err)
	}
	p.storeSession(uid, []byte("not-a-real-dek"))
	if _, err := p.Export(uid); !errors.Is(err, ErrNotConfigured) {
		t.Fatalf("unconfigured export = %v, want ErrNotConfigured", err)
	}

	if _, err := p.Setup(uid, pass); err != nil {
		t.Fatalf("setup: %v", err)
	}
	if _, _, err := p.Upsert(uid, Entry{AgentID: "a1", Protocol: "ssh", Port: 22, Username: "root", Password: "sekret-pass"}); err != nil {
		t.Fatalf("upsert: %v", err)
	}
	blob, err := p.Export(uid)
	if err != nil {
		t.Fatalf("export: %v", err)
	}
	if blob["format"] != "wingman-vault-v1" {
		t.Fatalf("export format = %v", blob["format"])
	}
	kdf, ok := blob["kdf"].(map[string]any)
	if !ok || kdf["salt"] == "" || kdf["wrapped_key"] == "" {
		t.Fatalf("export kdf bundle: %+v", blob["kdf"])
	}
	creds, ok := blob["credentials"].([]exportEntry)
	if !ok || len(creds) != 1 || creds[0].Username != "root" || creds[0].PasswordEnc == "" {
		t.Fatalf("export credentials: %+v", blob["credentials"])
	}
	if _, err := time.Parse(time.RFC3339, blob["exportedAt"].(string)); err != nil {
		t.Fatalf("exportedAt should be RFC3339: %v", err)
	}
	// 隐私默认：导出束整体序列化后不得含明文（口令也不得出现在错误里）
	raw, err := json.Marshal(blob)
	if err != nil {
		t.Fatalf("marshal export: %v", err)
	}
	if strings.Contains(string(raw), "sekret-pass") || strings.Contains(string(raw), pass) {
		t.Fatal("export must never contain plaintext secrets")
	}
}

func TestLocalLookupFallbackAndTamper(t *testing.T) {
	p := newLocalProv(t)
	const uid uint = 41
	if _, err := p.Setup(uid, "master-pass-1"); err != nil {
		t.Fatalf("setup: %v", err)
	}
	if _, _, err := p.Upsert(uid, Entry{AgentID: "a1", Protocol: "ssh", Port: 22, Username: "root", Password: "pw-ssh"}); err != nil {
		t.Fatalf("upsert: %v", err)
	}

	// port 0 归一到协议默认；显式同值端口精确命中
	if user, pw, _, err := p.Lookup(uid, "a1", "ssh", 0); err != nil || user != "root" || pw != "pw-ssh" {
		t.Fatalf("normalized lookup: %q %q %v", user, pw, err)
	}
	// 非默认端口无条目 → 回退默认端口条目（存取形态差兜底）
	if _, pw, _, err := p.Lookup(uid, "a1", "ssh", 2222); err != nil || pw != "pw-ssh" {
		t.Fatalf("fallback lookup: %q %v", pw, err)
	}
	if _, _, _, err := p.Lookup(uid, "a9", "ssh", 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("missing agent = %v, want ErrNotFound", err)
	}
	if _, _, _, err := p.Lookup(uid, "a1", "vnc", 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("missing protocol = %v, want ErrNotFound", err)
	}

	// 库中密文被手改：GCM 认证失败 → ErrDecrypt（口令错误同通道，这里直改库只可能是损坏）
	var cred models.RemoteCredential
	if err := p.db.Where("user_id = ?", uid).First(&cred).Error; err != nil {
		t.Fatalf("load cred: %v", err)
	}
	if err := p.db.Model(&cred).Update("password_enc", "deadbeef").Error; err != nil {
		t.Fatalf("tamper: %v", err)
	}
	if _, _, _, err := p.Lookup(uid, "a1", "ssh", 0); !errors.Is(err, ErrDecrypt) {
		t.Fatalf("tampered lookup = %v, want ErrDecrypt", err)
	}
}
