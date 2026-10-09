# Agent 注册 Token 认证设计（A3-P1）

> 日期：2026-09-20。
> 里程碑定位：`docs/android-agent-design.md` §1.2 A3 可靠性中的「token 认证」，
> 本文为其工程设计 + P1 实施。与该文 §8 的约定一致：A1 不半途引入鉴权，
> A3 一次性落完整链路（server 校验 + 双端 agent 携带 + 灰度开关）。

---

## 1. 目标与威胁模型

### 1.1 防什么

agent 端口（默认 8888）是 TCP 明文帧协议、register 无任何鉴权：任何能触达
该端口的主机都可伪造 `agent.register` 成为「合法 agent」，进而：

- 接收并执行 server 下发的任意 Lua 脚本（run_script）；
- 上报伪造的脚本输出 / 资源数据污染 Dashboard；
- 挤占真实 agent 的 agentId（后注册覆盖同名）。

P1 目标：**未持有效 token 的连接无法完成注册**。

### 1.2 不防什么（边界文档化）

| 不设防项 | 原因 | 演进 |
|----------|------|------|
| 通道窃听（token 明文过网） | 帧协议无 TLS；内网/局域网部署下嗅探风险可接受 | P2 challenge-response 免明文 / TLS 部署形态 |
| 中间人 | 同上 | TLS |
| 已持 token 的恶意 agent | token 即信任边界，泄露=轮换 | P2 per-agent token + 吊销 |
| Dashboard / HTTP API | 已有 JWT + RBAC（middleware 层），不在本文范围 | — |

### 1.3 与架构约束的关系

`docs/architecture-decisions.md` 五条约束均不触碰：鉴权只加在既有
agent→server 出站长链接的 register 消息上，无新监听面、无新消息类型。

---

## 2. 协议设计（复用现有帧，零新消息类型）

### 2.1 register 携带 token

`agent.register` Notify payload 增加顶层 `token` 字段：

```json
{ "type": "agent.register", "agentId": "android-pixel-8",
  "hostname": "Pixel 8", "platform": "android", "token": "wt_..." }
```

- token 放**顶层**而非 capabilities/metadata：它是鉴权凭证，不是能力元数据，
  server 校验逻辑直接读顶层字段，语义明确。
- 旧版 agent 不带 token：server 侧开关开启时视为无效（拒绝），开关关闭时
  完全兼容（现状不变）。

### 2.2 拒绝路径（复用 register_ack，agent 零改动）

校验失败时 server 回既有 `agent.register_ack`，`success:false`：

```json
{ "type": "agent.register_ack", "success": false, "error": "invalid or missing token" }
```

随后**立即断连**（未注册连接不允许继续停留在链路上）。

agent 端 `RemoteClient::handleRegisterAck`（`libs/agentcore/src/remote_client.cpp`）对
`success:false` 已有现成处理：置 `ConnectionState::Error`("Registration failed")
——**C++ 侧拒绝反馈零改动**，Android/桌面 UI 均能经既有状态回调感知。
指数退避重连会持续重试，设备侧表现为「连上即断」，配合本文 §5 的配置项，
部署者可直接定位为 token 错误。

### 2.3 时序

```
agent                                server
  │ TCP connect                       │
  │ ── Notify agent.register{token} ─▶│ token 开启？──▶ constant-time 比对
  │                                   │   失败：ack{success:false} → close
  │ ◀── Notify agent.register_ack ────│   成功：ack{success:true} → 入 Registry
```

---

## 3. Go Server 改动

| 文件 | 改动 |
|------|------|
| `internal/config/config.go` | `AgentTokens []string`，环境变量 `WINGMAN_AGENT_TOKENS`（逗号分隔）。**空 = 关闭鉴权（默认，向后兼容）** |
| `internal/agent/listener.go` | `FrameListener` 增 `agentTokens` + `SetAgentTokens()`（构造后注入，兼容既有调用方）；`handleRegister` 校验：失败 → ack success:false + 断连 + 日志，**不**入 Registry、不 set agentID |
| `main.go` | 启动时 `frameListener.SetAgentTokens(cfg.AgentTokens)` |

要点：

- **多 token 并存**：配置多个有效 token，支持平滑轮换（新旧并存期间双端
  依次更新，无停机窗口）。
- **constant-time 比较**：用 `crypto/subtle.ConstantTimeCompare` 逐个比对，
  防时序侧信道逐字节猜测。
- **拒绝即断连**：ack 先于 close（sendNotify 同步写，flush 后再 Close），
  保证 agent 能收到失败原因；未注册的 conn 断开后 readLoop 退出，
  agentID 为空则跳过 Unregister，`conns` 表以 connID 清理——既有清理路径
  自然兜住，无残留。
- 失败日志带来源 IP：`[Auth] register rejected from %s (agentId=%s)`。

### 3.1 轮换流程（运维约定）

1. server 配置追加新 token（新旧并存）→ 重启/热加载；
2. 各 agent 依次更新为新 token、重连（验证 Dashboard 在线）；
3. 确认全部 agent 已用新 token 后，移除旧 token 再重启一次。

---

## 4. Agent 端改动

### 4.1 共享层（apps/agent/RemoteClient，桌面与 Android 同源）

- `RemoteClient::setAuthToken(const std::string&)`：Impl 增 `authToken`，
  `sendRegister` 时写入 payload 顶层 `token`（空则不写字段，兼容不鉴权部署）。

### 4.2 桌面 runtime

- `RemoteClientConfig`（config.hpp）增 `registerToken`；
- INI 配置 `[remote] register_token = "..."`（agent_config.cpp 解析 + 保存）；
- `Agent::initRemoteClient()` 装配点调用 `setAuthToken`；
- GUI 入口：Tauri 设置页「远程注册配置」经本地 IPC
  `config.getRemote` / `config.setRemote` 读写 server 地址与 token，
  保存即热生效并写回配置文件（仅本地 IPC，无远程命令，见
  architecture-decisions.md 的 Remote Config Commands）。

### 4.3 Android

- `AndroidAgent::Config` 增 `authToken` → `setAuthToken`；
- Kotlin 链路：`WingmanJni.nativeStart(configJson)` 的 config 增 `authToken` 键
  （进入 C++ 后仍以 wire 字段 `token` 携带上报），`WingmanService` 从
  SharedPreferences 读 `serverToken` 传入；
- token 存储：P1 用 SharedPreferences（与 server 地址同级的信任级别，
  部署者手输）；✅ 已迁移 Android Keystore（2026-10-10，A3-P2）：
  `EncryptedSecretStore`（EncryptedSharedPreferences，AES256-GCM 主密钥
  入 AndroidKeyStore）+ 一次性明文迁移（`SecretStores.migrateToken`，
  JVM 单测覆盖；确保 secure 持有值后即清除明文残留；EncryptedSettings
  初始化失败回退明文库保可用）.

---

## 5. 部署形态与默认值

| 场景 | server 配置 | 行为 |
|------|-------------|------|
| 现有内网部署（默认） | 不设 `WINGMAN_AGENT_TOKENS` | 鉴权关闭，**完全向后兼容** |
| 公网可达 / 多租户内网 | 设置 token 列表 + 各 agent 配置同 token | 未授权注册被拒 |
| 轮换期 | 新旧 token 并存 | 双端依次迁移 |

**设计取向**：默认关闭而非默认强制。强制默认会打断所有现存部署（含
nightly 用户），鉴权属于部署者显式启用的能力；文档（本文 + README）明示
公网部署必须开启。

---

## 6. P2 演进

1. **challenge-response**（✅ Go server 已落地，2026-10-10；agent 侧接入见 §6.1.1）：
   register 携带顶层 `challenge: true` 且**不携明文 token** → server 下发
   Request `auth.challenge{nonce}`（32 字节 hex，5s 超时）→ agent 回
   `{"hmac": hex}`，**mac = HMAC-SHA256(key, nonce)，key = sha256hex(token)**：
   - agent 侧自行从本地明文推导密钥，token 全程不过网，消除 P1 的嗅探面；
   - 服务端 env 源以明文派生 sha256hex、DB 源直接用 TokenHash 列作密钥
     （两者同值）——无需存明文、无 schema 变更，DB 泄露威胁面与 §6.2 等同；
   - 验签命中路径与 §6.2 Verify 完全同口径：constant-time 比对、agentId
     绑定校验、LastSeenAt 刷新；
   - 实现要点：挑战在独立 goroutine 完成（handleRegister 由 readLoop 驱动，
     同步等 Response 会自锁），`challengeStarted`/`settled` 两个 CAS 门
     防并发双重注册与异步终态竞态。
   - agent 未实现 challenge 时继续走 §2.1 明文 token 路径（双模式并存，
     agent 侧按接入节奏切换）。

   §6.1.1 agent 侧接入（✅ 桌面链路已落地，2026-10-10；Android 见 §4.3）：
   - HMAC-SHA256 为 agentcore 自含实现（`hmac_sha256.hpp/.cpp`，纯 C++23
     标准库、零平台分支）——只为一次 HMAC 调用不值得引 OpenSSL（Android
     NDK 构建免补 openssl 包），单测以 RFC 4231 标准向量 + server 侧
     tokenMAC 同向量交叉验证（agentcore 测试 40 例全绿）；
   - `RemoteClientConfig.useChallengeAuth`（默认关 = P1 明文兼容）：开启后
     register 携 `challenge: true` 且明文 token 绝不过网；server 下发
     `auth.challenge` 由 `handleAuthChallenge` 拦截应答（鉴权凭证属于链路
     自身，不进 commandCallback 命令分发）；
   - 桌面配置 `[remote] challenge_auth`（agent_config.cpp 解析/写回 +
     roundtrip 单测），GUI 设置页「远程注册配置」增 challenge 开关
     （config.getRemote/setRemote 链路透传，vitest 覆盖）。
2. **per-agent token + 管理面**（✅ 已落地，2026-10-10）：
   - `models.AgentToken` 入 DB（label/sha256 哈希/前缀/可选 agentId 绑定/
     createdBy/revokedAt/lastSeenAt），明文只在签发响应返回一次；
   - `agent.TokenStore`：Create/Verify/Revoke/List/AnyExists，校验按哈希
     constant-time 查库、命中后校验绑定并刷新 lastSeenAt；
   - listener 双源并存（§6.3 演进只加不改的落点）：env 白名单 ∪ DB 源，
     任一命中即放行；**DB 源启用口径 = 存在签发记录（含已吊销，
     fail-closed）**——吊销全部 token 不会静默关闭鉴权；env 清空后即纯
     DB 管理面模式；
   - HTTP 管理面 `GET/POST/DELETE /api/agent-tokens`（`agenttokens:manage`
     权限码，内置角色未授予、默认仅 admin），签发/吊销落审计
     （`agenttoken.create` / `agenttoken.revoke`）；注册拒绝仍走 server
     日志（不进审计表，防爆破刷表）；
   - Dashboard「系统管理 → 注册 Token」页（签发弹窗一次性展示明文 +
     吊销确认）。
3. **TLS 部署形态**（未实施）：server 前置 TLS 终结或帧协议升级 TLS，
   一并解决明文命令/脚本下发。

P1 刻意不引入以上任何一项的半成品：校验点（handleRegister）、token 配置
形态（环境变量列表）、agent 携带字段（顶层 token）在 P2 全部保持不变，
演进只加不改。

---

## 7. 测试矩阵

| 层 | 用例 | 本机可验 |
|----|------|----------|
| config | `WINGMAN_AGENT_TOKENS` 解析（单/多/空/含空白） | ✅ go test |
| listener | token 正确 → 注册成功；错误/缺失 + 开启 → ack success:false + 断连 + 不入 Registry；关闭开关 → 不校验 | ✅ go test |
| integration | 开启 token 的真实链路：sim agent 带正确 token 上线、错误 token 被拒 | ✅ go test |
| token store（P2） | 签发/校验/吊销/绑定/列表不泄哈希；fail-closed（吊销全部仍拒） | ✅ go test |
| listener（P2） | DB token 放行；吊销后拒绝并断连；双源并存；空库不启用鉴权（兼容） | ✅ go test |
| listener（P2 challenge） | env/DB token 挑战应答通过（LastSeenAt 刷新）；错误 HMAC / 绑定不符 / 超时不应答 → 拒绝断连；挑战中重复 register → CAS 拒绝 | ✅ go test |
| HTTP API（P2） | 签发响应明文仅一次；列表不泄哈希；吊销幂等 + 审计 | ✅ go test |
| C++ | setAuthToken → register payload 含 token；未设置 → 不含字段（编译 + 桌面回归） | ✅ |
| C++（P2 challenge） | HMAC RFC 4231 向量 + server tokenMAC 同向量；challenge 模式 register 携 challenge:true 不携 token；auth.challenge 应答 hmac 正确；无 token 回空 hmac；默认模式不携 challenge 字段 | ✅ gtest |
| 桌面配置（P2 challenge） | `[remote] challenge_auth` 解析 + save/load roundtrip；GUI 开关读写链路 | ✅ gtest + vitest |
| Android | configJson 传递（代码交付，真机验收见 apps/android/README） | ❌ |

## 8. 实施清单（P1，对应本次提交）

- [x] 本设计文档
- [x] Go：config + listener 校验 + main 接线 + 单测/集成测试
- [x] C++：RemoteClient::setAuthToken + 桌面配置项 + Android Config
- [x] Kotlin：configJson `authToken` 键 + Service 读取
- [x] 文档：android-agent-design.md §8 引用、apps/android/README 配置说明
