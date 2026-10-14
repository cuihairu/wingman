# 密钥保险箱 Provider 抽象与凭据接入点盘点

> 依据根 todo.md 2026-10-11「密码箱抽象与可替换提供商」（persona 新升级已实现密码箱，各项目密码箱应抽象接口、支持替换提供商）。
> 实现落位：`orchestrator/server/internal/vault`（2026-10-13）。
>
> **红线**：密钥不进仓库；解锁口令与明文凭据不落日志、不出现在错误消息。

## 1. Provider 统一接口

解锁/读/写/列/删除/导出/连接取用收敛在一个接口面（`internal/vault/vault.go`）：

| 方法 | 语义 | 失败 sentinel |
|------|------|---------------|
| `Name()` | provider 名（`local` 等） | — |
| `Status(userID)` | 配置/解锁状态（弹窗探测，不泄密） | — |
| `Setup(userID, masterPassword)` | 首次设置主口令（设置即解锁） | `ErrWeakMaster` / `ErrAlreadyConfigured` |
| `Unlock(userID, masterPassword)` | 校验主口令并进入解锁态 | `ErrNotConfigured` / `ErrWrongMaster` |
| `ChangePassword(userID, current, next)` | 换主口令（密文不动） | `ErrWrongCurrent` / `ErrWeakMaster` / `ErrNotConfigured` |
| `Lock(userID)` | 立即锁回（幂等） | — |
| `List(userID)` | 凭据元数据列表（无密文无明文） | — |
| `Upsert(userID, entry)` | 保存/更新（秘密留空 = 保留旧值） | `ErrLocked` / `ErrNeedSecret` |
| `Delete(userID, entryID)` | 删除（非本人条目按不存在） | `ErrLocked` / `ErrNotFound` |
| `Export(userID)` | 导出密文束（明文永不导出，格式由实现定义） | `ErrLocked` / `ErrNotConfigured` |
| `Lookup(userID, agentID, protocol, port)` | 连接快捷调用：取目标凭据并解密（票据 useSaved 注入） | `ErrLocked` / `ErrNotFound` / `ErrDecrypt` |

分工边界（口径不散落）：

- **provider 实现负责**：主口令策略（最小长度等）、加密与落库、解锁会话（驻留 TTL、
  空闲自动锁回）、导出格式、取用解密；
- **HTTP 适配层负责**（`internal/handlers/vault.go`）：请求形状校验（非空、协议白名单、
  端口归一、export 显式确认）、**审计**（`WriteAuditLog` 的 `vault.*` 事件统一在本层记，
  provider 不重复记）、sentinel → HTTP 状态码映射（401/404/409/423/400）。

sentinel 错误一律静态文案（口令/明文不进 error），文案保持与既有前端识别兼容
（`vault locked` / `not configured` 等子串匹配）。

## 2. 默认实现 `local`（自托管零外部依赖）

加密结构（`models/vault.go` 头注释）：

```
主口令 ──PBKDF2-HMAC-SHA256(salt, 600k)──▶ KEK
随机 32B DEK ──AES-256-GCM(KEK)──▶ WrappedKey（落库 vault_masters）
凭据 Password/PrivateKey ──AES-256-GCM(DEK)──▶ 密文（落库 remote_credentials）
```

- 加密原语在 `internal/security`（纯函数），`LocalProvider` 是它们唯一接线处；
- DEK 只在解锁期间驻留本进程内存，**空闲自动锁回**（每次取用/管理刷新计时，
  默认 30 分钟，可配）；进程重启天然全锁；
- 主口令最小长度 8（`vault.MinMasterLen`；KDF 轮数扛离线爆破）；
- 换主口令只重包裹 DEK，已存密文全部继续有效；
- 未存主口令摘要——解锁 = 解开 WrappedKey，GCM 认证失败即口令错（AEAD 下与
  密文被篡改不可区分，统一折叠为 `ErrWrongMaster`）。

## 3. 切换机制（验收①：只改配置换实现）

- 启动装配（`main.go`）：`vault.Activate(cfg.VaultProvider, vault.Options{DB, Config})`；
  未知名/构造失败 **启动即炸**，不静默降级；
- 端点与业务代码（含 guacamole 票据注入）每请求经 `vault.Active()` 现取，不缓存实现
  引用——业务面零改动；
- 切换 = 改 `WINGMAN_VAULT_PROVIDER` + **重启**（进程级单选；不做运行中热切换——
  解锁会话驻留在实现内部，热切换会悬空已解锁会话）；
- 新增 provider：实现 `vault.Provider` → `init()` 里 `vault.Register(name, factory)` →
  补配置节文档。外部密码箱（persona 形态）按接口注册即接入，无需触碰任何业务代码。

## 4. 配置（逐字段）

server 配置为纯环境变量（无配置文件），保险箱相关字段：

| 字段 | 类型 | 默认值 | 含义 | 示例 |
|------|------|--------|------|------|
| `WINGMAN_VAULT_PROVIDER` | string | `local` | 保险箱 provider 选择；可选值见 `internal/vault` 注册表，未知名启动报错退出 | `local` |
| `WINGMAN_VAULT_LOCAL_SESSION_TTL` | duration（Go `time.ParseDuration`，须 > 0） | `30m` | `local` 实现的解锁空闲自动锁回时长（status 的 `autoLockAfterS` 倒计时基准）；非法值启动报错 | `45m` |

命名规则：`WINGMAN_VAULT_<PROVIDER>_*` 为各 provider 的配置节，去公共前缀
`WINGMAN_VAULT_` 后原样透传给对应实现（键保留 PROVIDER 段，实现只读自己的段）。

示例（占位值，非真实密钥）：

```bash
export WINGMAN_VAULT_PROVIDER=local
export WINGMAN_VAULT_LOCAL_SESSION_TTL=30m
```

## 5. 凭据接入点盘点（2026-10-13 实测，验收②）

扫描口径：`scripts/check_vault_touchpoints.sh`（CI `Vault Touchpoint Scan` job 守护）——
白名单之外的 `os.Getenv` 凭据形直读即失败。实测结论：

| 层 | 接入点 | 位置 | 能否改走保险箱 |
|----|--------|------|----------------|
| 自举层 | `WINGMAN_JWT_SECRET` | `config.go`、`middleware/auth.go` | **否**：保险箱 API 自身靠 JWT 鉴权，密钥必须先于保险箱存在（自举循环） |
| 自举层 | `WINGMAN_AGENT_TOKENS` | `config.go` | **否**：agent 注册鉴权先于保险箱可用；per-agent token 已有 DB 源（A3-P2，fail-closed） |
| 自举层 | `WINGMAN_ADMIN_PASSWORD` | `handlers/auth.go` 引导 | **否**：首个管理员创建时尚无用户，亦无保险箱可查 |
| runtime 本地 | `WINGMAN_SCRIPT_PASSWORD` / `WINGMAN_PACK_PASSWORD` | `apps/agent`（本机 env） | **不属 server 保险箱面**：runtime 是独立本机进程（架构硬约束 outbound-only），本地工具口令机器本地即终态 |
| 保险箱管理面 | RDP/VNC/SSH 远程桌面凭据 | `models.RemoteCredential` | **已接入 ✓**：存取全走 VaultProvider，明文只在票据签发那一刻解密进 connect 参数 |

结论：

- server 侧**无** webhook secret / 第三方平台 token / 第三方 API key 明文面（实测
  grep 0 命中）；
- 现存 env 直读全部为自举层，均有「先于保险箱存在」的结构性理由（上表）；
- 未来新增凭据面必须走 VaultProvider——CI 扫描拦截白名单外的直读。

## 6. 验收留证（硬口径）

- **① 真实切换**：`internal/handlers/vault_provider_test.go`
  `TestVaultProviderSwitchByConfig`——注册语义全异的 stub 第二实现，仅 `Activate`
  （生产里由 `WINGMAN_VAULT_PROVIDER` 驱动）即接管同一批端点（status/unlock/取用），
  切回 local 语义复原；形状校验与审计留在适配层不随 provider 走。业务代码零改动。
- **② 接入点扫描**：`./scripts/check_vault_touchpoints.sh` → `clean (boot-tier
  allowlist only)`；CI `Vault Touchpoint Scan` job 持续守护。
- **③ 测试绿**：`cd orchestrator/server && go test -race -count=1 ./...` 全绿；
  commit 英文简短；不打 tag、不发版。
