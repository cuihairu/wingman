> [中文](agent-token-auth-design.md) | **English**

# Agent Registration Token Authentication Design (A3-P1)

> Date: 2026-09-20.
> Milestone positioning: "Token authentication" within §1.2 A3 Reliability of `docs/android-agent-design.md`;
> this document is its engineering design + P1 implementation. Consistent with §8 of that document:
> A1 does not introduce authentication halfway, A3 lands the full chain in one pass
> (server-side verification + token carried by agents on both ends + gradual rollout switch).

---

## 1. Goals and Threat Model

### 1.1 What It Defends Against

The agent port (default 8888) is a TCP plaintext frame protocol with no authentication on
register at all: any host that can reach that port can forge an `agent.register` to
become a "legitimate agent", thereby:

- receiving and executing any Lua script dispatched by the server (run_script);
- reporting forged script output / resource data to pollute the Dashboard;
- crowding out the agentId of a real agent (a later registration overwrites the same name).

P1 goal: **a connection without a valid token cannot complete registration**.

### 1.2 What It Does Not Defend Against (Boundary Documented)

| Item Not Defended Against | Reason | Evolution |
|----------|------|------|
| Channel eavesdropping (token travels in plaintext over the wire) | The frame protocol has no TLS; under intranet/LAN deployments the sniffing risk is acceptable | P2 challenge-response avoids plaintext / TLS deployment form |
| Man-in-the-middle | Same as above | TLS |
| A malicious agent already holding a token | The token is itself the trust boundary; leak = rotate | P2 per-agent token + revocation |
| Dashboard / HTTP API | JWT + RBAC already exists (middleware layer), out of scope for this document | — |

### 1.3 Relation to the Architecture Constraints

None of the five constraints in `docs/architecture-decisions.md` is touched: authentication
is only added to the register message of the existing agent→server outbound long link,
with no new listening surface and no new message type.

---

## 2. Protocol Design (Reusing Existing Frames, Zero New Message Types)

### 2.1 Register Carries the Token

The `agent.register` Notify payload gains a top-level `token` field:

```json
{ "type": "agent.register", "agentId": "android-pixel-8",
  "hostname": "Pixel 8", "platform": "android", "token": "wt_..." }
```

- The token goes at the **top level**, not in capabilities/metadata: it is an
  authentication credential, not capability metadata; the server verification logic reads
  the top-level field directly, so the semantics are clear.
- Old agents without a token: when the server-side switch is on, it is treated as invalid
  (rejected); when the switch is off, it is fully compatible (current behavior unchanged).

### 2.2 Rejection Path (Reusing register_ack, Zero Agent-Side Changes)

On verification failure the server replies with the existing `agent.register_ack`,
`success:false`:

```json
{ "type": "agent.register_ack", "success": false, "error": "invalid or missing token" }
```

Then **disconnects immediately** (an unregistered connection is not allowed to stay on
the link).

The agent side `RemoteClient::handleRegisterAck` (`libs/agentcore/src/remote_client.cpp`)
already has existing handling for `success:false`: it sets
`ConnectionState::Error` ("Registration failed")
—**zero changes on the C++ rejection feedback path**; both Android and desktop UI can
observe it through the existing state callback. Exponential-backoff reconnection keeps
retrying; on the device side this shows up as "connects then immediately drops", and
with the configuration items in §5 of this document, deployers can directly diagnose it
as a wrong token.

### 2.3 Sequence

```
agent                                server
  │ TCP connect                       │
  │ ── Notify agent.register{token} ─▶│ token 开启？──▶ constant-time 比对
  │                                   │   失败：ack{success:false} → close
  │ ◀── Notify agent.register_ack ────│   成功：ack{success:true} → 入 Registry
```

---

## 3. Go Server Changes

| File | Change |
|------|------|
| `internal/config/config.go` | Add `AgentTokens []string`, environment variable `WINGMAN_AGENT_TOKENS` (comma-separated). **Empty = authentication disabled (default, backward compatible)** |
| `internal/agent/listener.go` | `FrameListener` gains `agentTokens` + `SetAgentTokens()` (injected after construction, compatible with existing callers); `handleRegister` verifies: failure → ack success:false + disconnect + log, does **not** enter the Registry, does not set agentID |
| `main.go` | At startup call `frameListener.SetAgentTokens(cfg.AgentTokens)` |

Key points:

- **Multiple tokens coexist**: configure multiple valid tokens to support smooth rotation
  (during the coexistence period both ends update in turn, with no downtime window).
- **Constant-time comparison**: use `crypto/subtle.ConstantTimeCompare` to compare one by
  one, preventing byte-by-byte guessing via timing side channels.
- **Reject means disconnect**: the ack comes before the close (sendNotify writes
  synchronously, Close after flush), guaranteeing the agent receives the failure reason;
  after an unregistered conn is closed, its readLoop exits; skip Unregister when agentID
  is empty, and the `conns` table is cleaned by connID—the existing cleanup path catches
  it naturally, with no residue.
- Failure log carries the source IP: `[Auth] register rejected from %s (agentId=%s)`.

### 3.1 Rotation Procedure (Operational Convention)

1. Append the new token to the server configuration (new and old coexist) → restart / hot reload;
2. Update each agent in turn to the new token and reconnect (verify the Dashboard is online);
3. After confirming all agents use the new token, remove the old token and restart once more.

---

## 4. Agent-Side Changes

### 4.1 Shared Layer (apps/agent/RemoteClient, same source for Desktop and Android)

- `RemoteClient::setAuthToken(const std::string&)`: the Impl gains an `authToken`; when
  `sendRegister` runs, it writes the top-level `token` field into the payload (empty means
  the field is not written, compatible with deployments without authentication).

### 4.2 Desktop Runtime

- `RemoteClientConfig` (config.hpp) gains `registerToken`;
- INI configuration `[remote] register_token = "..."` (agent_config.cpp parses + saves);
- `Agent::initRemoteClient()` calls `setAuthToken` at the assembly point;
- GUI entry: the Tauri settings page "Remote Registration Configuration" reads/writes the
  server address and token via local IPC `config.getRemote` / `config.setRemote`;
  saving takes effect immediately and is written back to the configuration file (local IPC
  only, no remote command, see Remote Config Commands in architecture-decisions.md).

### 4.3 Android

- `AndroidAgent::Config` gains `authToken` → `setAuthToken`;
- Kotlin chain: the config of `WingmanJni.nativeStart(configJson)` gains an `authToken`
  key (after entering C++ it is still carried in the wire field `token` for reporting),
  `WingmanService` reads `serverToken` from SharedPreferences and passes it in;
- Token storage: P1 uses SharedPreferences (same trust level as the server address,
  entered by the deployer); ✅ migrated to Android Keystore (2026-10-10, A3-P2):
  `EncryptedSecretStore` (EncryptedSharedPreferences, AES256-GCM master key in
  AndroidKeyStore) + one-time plaintext migration (`SecretStores.migrateToken`, covered
  by JVM unit tests; after confirming the secure store holds the value, the plaintext
  residue is cleared immediately; if EncryptedSettings initialization fails, fall back to
  the plaintext store to preserve availability);
- Challenge integration (✅ 2026-10-10, A3-P2 §6.1): `AndroidAgent::Config` gains
  `useChallengeAuth` (jni_bridge parses the `challengeAuth` key → passed through to the
  `RemoteClient` config; HMAC uses agentcore's self-contained implementation, so the
  Android build needs no additional openssl package); the Kotlin side has
  `AgentPrefs.KEY_CHALLENGE_AUTH` (default false) + a Challenge toggle in the main UI
  (layout/string/read-write chain), and `WingmanService.startCore` reads the same key into
  the config.

---

## 5. Deployment Forms and Defaults

| Scenario | Server Configuration | Behavior |
|------|-------------|------|
| Existing intranet deployment (default) | `WINGMAN_AGENT_TOKENS` not set | Authentication disabled, **fully backward compatible** |
| Publicly reachable / multi-tenant intranet | Set a token list + configure the same token on each agent | Unauthorized registration is rejected |
| Rotation period | New and old tokens coexist | Both ends migrate in turn |

**Design choice**: default off rather than default enforced. Enforcing it by default
would break all existing deployments (including nightly users); authentication is a
capability the deployer enables explicitly; the documentation (this document + README)
states explicitly that public deployments must enable it.

---

## 6. P2 Evolution

1. **challenge-response** (✅ landed on the Go server, 2026-10-10; agent-side integration
   see §6.1.1): register carries a top-level `challenge: true` and does **not** carry the
   plaintext token → the server dispatches Request `auth.challenge{nonce}` (32-byte hex,
   5s timeout) → the agent replies `{"hmac": hex}`, **mac = HMAC-SHA256(key, nonce),
   key = sha256hex(token)**:
   - The agent derives the key itself from the local plaintext; the token never travels
     over the wire, eliminating the P1 sniffing surface;
   - On the server side, the env source derives sha256hex from the plaintext, and the DB
     source uses the TokenHash column directly as the key (both have the same value)—no
   plaintext storage, no schema change, and the DB leak threat surface is equivalent to §6.2;
   - The signature verification hit path is exactly the same as the §6.2 Verify:
     constant-time comparison, agentId binding check, LastSeenAt refresh;
   - Implementation notes: the challenge completes in an independent goroutine (handleRegister
     is driven by readLoop, and synchronously waiting for a Response would self-deadlock);
     two CAS gates `challengeStarted`/`settled` prevent concurrent double registration and
     async terminal-state races.
   - When an agent has not implemented challenge, it continues to use the §2.1 plaintext
   token path (both modes coexist, the agent side switches at its own integration pace).

   §6.1.1 Agent-side integration (✅ desktop chain landed, 2026-10-10; Android see §4.3):
   - HMAC-SHA256 is agentcore's self-contained implementation (`hmac_sha256.hpp/.cpp`,
     pure C++23 standard library, zero platform branches)—a single HMAC call is not worth
     pulling in OpenSSL (Android NDK builds need no additional openssl package); unit tests
     cross-validate with RFC 4231 standard vectors + the server-side tokenMAC on the same
     vectors (all 40 agentcore test cases green);
   - `RemoteClientConfig.useChallengeAuth` (default off = P1 plaintext compatibility): when
     enabled, register carries `challenge: true` and the plaintext token never travels over
     the wire; the server dispatches `auth.challenge`, which `handleAuthChallenge` intercepts
     and answers (authentication credentials belong to the link itself, not dispatched into
     commandCallback);
   - Desktop configuration `[remote] challenge_auth` (agent_config.cpp parse/write back +
     roundtrip unit test), GUI settings page "Remote Registration Configuration" gains a
     Challenge toggle (config.getRemote/setRemote chain passthrough, covered by vitest).
2. **per-agent token + management plane** (✅ landed, 2026-10-10):
   - `models.AgentToken` enters the DB (label/sha256 hash/prefix/optional agentId binding/
     createdBy/revokedAt/lastSeenAt); the plaintext is returned only once in the issuance
     response;
   - `agent.TokenStore`: Create/Verify/Revoke/List/AnyExists; verification queries the DB
     constant-time by hash, and after a hit verifies the binding and refreshes lastSeenAt;
   - listener keeps both sources coexisting (the landing point for the §6.3 evolution which
     only adds, never changes): env whitelist ∪ DB source, any hit passes; **the DB source
     is considered enabled when an issuance record exists (including revoked ones,
     fail-closed)**—revoking all tokens does not silently disable authentication; after the
     env is cleared, it is pure DB management-plane mode;
   - HTTP management plane `GET/POST/DELETE /api/agent-tokens` (permission code
     `agenttokens:manage`, not granted to built-in roles, default admin only), issuance/
     revocation are audited (`agenttoken.create` / `agenttoken.revoke`); registration
     rejection still goes through server logs (not into the audit table, to prevent
     brute-force flooding);
   - Dashboard "System Management → Registration Token" page (issuance dialog shows the
     plaintext once + revocation confirmation).
3. **TLS deployment form** (Form A landed 2026-10-19: `deployments/tls/`—an nginx
   `stream{}` TLS-termination compose + operations doc, zero code on the agent side;
   verified locally end to end with TLSv1.3 termination and byte-level frame passthrough,
   see the verification record in that directory's README. Form B not implemented,
   pending a project proposal—comparison and decision between the two forms below):
   the frame protocol is currently bare TCP (agentcore `asio::ip::tcp::socket` ↔ Go
   `net.Listener`); TLS cannot be transparently layered by a reverse proxy the way HTTP can,
   so a cost/benefit comparison of two forms:
   - **Form A: TLS termination at the TCP layer (deployment form, the transitional
     solution first)**—nginx `stream {}` or stunnel terminates TLS in front of the server
     and forwards pure TCP to `server:8888`; the agent side needs zero changes, only the
     connection address changes. caddy/Let's Encrypt manages the certificate (if a domain
     exists) or self-signed (if no domain). Benefit: command/script dispatch is encrypted
     end to end; boundary: the agent does not validate the certificate (the frame protocol
     has no SNI/certificate-validation hook), so it defends against passive sniffing but not
     active MITM—the token is already challenge-HMAC'd, so a MITM also cannot obtain the
     token; acceptable as an intranet/demo form. Cost: one compose + operations docs, zero
     code.
   - **Form B: native TLS in the frame protocol (end state, needs a project proposal)**—Go
     `crypto/tls` Listener + agentcore `asio::ssl::stream` (vcpkg openssl, desktop) +
     Android `SSLSocket`/NDK openssl (conflicts with the §6.1.1 "no OpenSSL" approach;
     pulling in the package on the Android side is the main cost). Certificate validation
     uses the **PIN form** (self-signed root fingerprint embedded in configuration, no
     domain needed, no ACME, fits intranet topology), eliminating any "disable validation"
     configuration option. It involves full-path changes to reconnection/proxy_tunnel/timeout
     + a test matrix for both ends, and is proposed as a project after review through the
     architecture decision process.
   - **Decision**: A lands immediately as a demo/intranet deployment guide (✅ 2026-10-19,
     `deployments/tls/`); B is a
     prerequisite for public production, and together with "Dashboard HTTPS" goes into the
     deployment security milestone; it is not forcibly bundled into the P2 token project.

P1 deliberately does not introduce a half-finished version of any of the above: the
verification point (handleRegister), the token configuration form (environment variable
list), and the agent-carried field (top-level token) all remain unchanged in P2; the
evolution only adds, never changes.

---

## 7. Test Matrix

| Layer | Case | Verifiable Locally |
|-------|------|----------|
| config | `WINGMAN_AGENT_TOKENS` parsing (single/multiple/empty/with whitespace) | ✅ go test |
| listener | correct token → registration succeeds; wrong/missing + switch on → ack success:false + disconnect + does not enter Registry; switch off → no verification | ✅ go test |
| integration | real link with token enabled: sim agent with correct token comes online, wrong token rejected | ✅ go test |
| token store (P2) | issuance/verification/revocation/binding/list does not leak the hash; fail-closed (still rejects after revoking all) | ✅ go test |
| listener (P2) | DB token passes; rejected and disconnected after revocation; both sources coexist; empty DB does not enable authentication (compatible) | ✅ go test |
| listener (P2 challenge) | env/DB token challenge response passes (LastSeenAt refreshed); wrong HMAC / binding mismatch / timeout without response → rejected and disconnected; repeated register during challenge → CAS rejection | ✅ go test |
| HTTP API (P2) | issuance response shows plaintext only once; list does not leak the hash; idempotent revocation + audit | ✅ go test |
| C++ | setAuthToken → register payload contains token; not set → field absent (compile + desktop regression) | ✅ |
| C++ (P2 challenge) | HMAC RFC 4231 vectors + server tokenMAC on the same vectors; challenge mode register carries challenge:true and no token; auth.challenge response hmac correct; no token returns empty hmac; default mode carries no challenge field | ✅ gtest |
| Desktop configuration (P2 challenge) | `[remote] challenge_auth` parsing + save/load roundtrip; GUI toggle read-write chain | ✅ gtest + vitest |
| Android | configJson passing (code delivered, real-device acceptance see apps/android/README) | ❌ |

## 8. Implementation Checklist (P1, corresponding to this commit)

- [x] This design document
- [x] Go: config + listener verification + main wiring + unit/integration tests
- [x] C++: RemoteClient::setAuthToken + desktop configuration item + Android Config
- [x] Kotlin: configJson `authToken` key + Service read
- [x] Docs: android-agent-design.md §8 reference, apps/android/README configuration notes
