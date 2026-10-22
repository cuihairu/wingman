> [中文](vault-provider-design.md) | **English**

# Secret Vault Provider Abstraction and Credential Touchpoint Inventory

> Based on the root todo.md item dated 2026-10-11, "vault abstraction and swappable providers" (the new persona upgrade has implemented a vault; each project's vault should expose an abstract interface and support swappable providers).
> Implementation location: `orchestrator/server/internal/vault` (2026-10-13).
>
> **Red line**: secrets never enter the repository; unlock passwords and plaintext credentials never land in logs and never appear in error messages.

## 1. Unified Provider Interface

Unlock/read/write/list/delete/export/lookup-for-connection are collapsed into a single interface surface (`internal/vault/vault.go`):

| Method | Semantics | Failure sentinel |
|------|------|---------------|
| `Name()` | provider name (`local`, etc.) | — |
| `Status(userID)` | configuration/unlock status (probe via popup, no secret leakage) | — |
| `Setup(userID, masterPassword)` | initial setup of the master password (setup unlocks) | `ErrWeakMaster` / `ErrAlreadyConfigured` |
| `Unlock(userID, masterPassword)` | verify the master password and enter the unlocked state | `ErrNotConfigured` / `ErrWrongMaster` |
| `ChangePassword(userID, current, next)` | change the master password (ciphertext stays put) | `ErrWrongCurrent` / `ErrWeakMaster` / `ErrNotConfigured` |
| `Lock(userID)` | lock back immediately (idempotent) | — |
| `List(userID)` | list of credential metadata (no ciphertext, no plaintext) | — |
| `Upsert(userID, entry)` | save/update (empty secret = keep the old value) | `ErrLocked` / `ErrNeedSecret` |
| `Delete(userID, entryID)` | delete (entries not belonging to the user are treated as nonexistent) | `ErrLocked` / `ErrNotFound` |
| `Export(userID)` | export the ciphertext bundle (plaintext is never exported; format is defined by the implementation) | `ErrLocked` / `ErrNotConfigured` |
| `Lookup(userID, agentID, protocol, port)` | quick call for connections: fetch the target credential and decrypt it (the ticket useSaved is injected here) | `ErrLocked` / `ErrNotFound` / `ErrDecrypt` |

Division of responsibilities (keep the contract in one place):

- **The provider implementation is responsible for**: master-password policy (minimum length, etc.), encryption and persistence, the unlock session (resident TTL,
  automatic lock on idle), export format, and decryption at lookup;
- **The HTTP adapter layer is responsible for** (`internal/handlers/vault.go`): request shape validation (non-empty, protocol whitelist,
  port normalization, explicit confirmation for export), **auditing** (all `vault.*` events of `WriteAuditLog` are recorded in this layer,
  the provider does not duplicate the records), and sentinel → HTTP status code mapping (401/404/409/423/400).

Sentinel errors always use static messages (passwords/plaintext never enter the error), and the wording stays compatible with existing frontend recognition
(substring matches such as `vault locked` / `not configured`).

## 2. Default Implementation `local` (self-hosted, zero external dependencies)

Encryption structure (header comment of `models/vault.go`):

```
主口令 ──PBKDF2-HMAC-SHA256(salt, 600k)──▶ KEK
随机 32B DEK ──AES-256-GCM(KEK)──▶ WrappedKey（落库 vault_masters）
凭据 Password/PrivateKey ──AES-256-GCM(DEK)──▶ 密文（落库 remote_credentials）
```

- The encryption primitives live in `internal/security` (pure functions); `LocalProvider` is their only wiring point;
- The DEK resides in this process's memory only while unlocked, with **automatic lock on idle** (every lookup/management call refreshes the timer,
  default 30 minutes, configurable); a process restart locks everything back naturally;
- Minimum master-password length is 8 (`vault.MinMasterLen`; the KDF rounds are chosen to withstand offline brute force);
- Changing the master password only re-wraps the DEK; all stored ciphertext remains valid;
- No master-password digest is stored — unlocking means unwrapping the WrappedKey, and GCM authentication failure means the password is wrong (under AEAD this is indistinguishable from ciphertext tampering, so both collapse into `ErrWrongMaster`).

## 3. Switching Mechanism (Acceptance ①: change only configuration to swap implementations)

- Startup wiring (`main.go`): `vault.Activate(cfg.VaultProvider, vault.Options{DB, Config})`;
  an unknown name or a construction failure **crashes at startup**, no silent degradation;
- Endpoints and business code (including guacamole ticket injection) fetch the current provider per request via `vault.Active()` and never cache an
  implementation reference — zero changes on the business plane;
- Switching = change `WINGMAN_VAULT_PROVIDER` + **restart** (process-level single selection; no hot swap while running —
  the unlock session is resident inside the implementation, and a hot swap would leave unlocked sessions dangling);
- Adding a provider: implement `vault.Provider` → call `vault.Register(name, factory)` in `init()` →
  document the configuration section. External vaults (persona style) are wired in by registering against the interface, without touching any business code.

## 4. Configuration (field by field)

The server configuration is pure environment variables (no config file); vault-related fields:

| Field | Type | Default | Meaning | Example |
|------|------|--------|------|------|
| `WINGMAN_VAULT_PROVIDER` | string | `local` | Vault provider selection; possible values are listed in the `internal/vault` registry; an unknown name fails startup and exits | `local` |
| `WINGMAN_VAULT_LOCAL_SESSION_TTL` | duration (Go `time.ParseDuration`, must be > 0) | `30m` | The idle-auto-lock duration of the `local` implementation's unlock session (the base for the `autoLockAfterS` countdown in status); an invalid value fails at startup | `45m` |

Naming rule: `WINGMAN_VAULT_<PROVIDER>_*` are each provider's configuration section. After stripping the common prefix
`WINGMAN_VAULT_`, the remainder is passed through verbatim to the corresponding implementation (the key keeps the PROVIDER segment, and the implementation only reads its own section).

Example (placeholder values, not real secrets):

```bash
export WINGMAN_VAULT_PROVIDER=local
export WINGMAN_VAULT_LOCAL_SESSION_TTL=30m
```

## 5. Credential Touchpoint Inventory (measured 2026-10-13, Acceptance ②)

Scan scope: `scripts/check_vault_touchpoints.sh` (guarded by the CI `Vault Touchpoint Scan` job) —
any direct read of a credential-shaped `os.Getenv` outside the allowlist fails. Measured conclusions:

| Layer | Touchpoint | Location | Can it move to the vault |
|----|--------|------|----------------|
| Bootstrap layer | `WINGMAN_JWT_SECRET` | `config.go`, `middleware/auth.go` | **No**: the vault API itself relies on JWT for authentication; the secret must exist before the vault does (bootstrap loop) |
| Bootstrap layer | `WINGMAN_AGENT_TOKENS` | `config.go` | **No**: agent registration authentication happens before the vault is available; the per-agent token already has a DB source (A3-P2, fail-closed) |
| Bootstrap layer | `WINGMAN_ADMIN_PASSWORD` | bootstrap in `handlers/auth.go` | **No**: at the moment the first administrator is created there is no user yet, and no vault to query |
| runtime local | `WINGMAN_SCRIPT_PASSWORD` / `WINGMAN_PACK_PASSWORD` | `apps/agent` (local machine env) | **Not in the server vault plane**: the runtime is an independent local process (architecture hard constraint: outbound-only); local tool passwords are final-state machine-locally |
| Vault management plane | RDP/VNC/SSH remote-desktop credentials | `models.RemoteCredential` | **Wired in ✓**: all reads and writes go through VaultProvider; plaintext is decrypted only at the moment a ticket is issued, into the connect parameters |

Conclusions:

- There is **no** plaintext plane for webhook secrets / third-party platform tokens / third-party API keys on the server side
  (measured grep: 0 hits);
- All existing direct env reads are bootstrap-layer ones and all have a structural reason to "exist before the vault" (table above);
- Any future credential plane must go through VaultProvider — the CI scan blocks direct reads outside the allowlist.

## 6. Acceptance Evidence (hard requirements)

- **① Real switching**: `internal/handlers/vault_provider_test.go`
  `TestVaultProviderSwitchByConfig` — registers a stub second implementation with completely different semantics, and `Activate`
  alone (driven in production by `WINGMAN_VAULT_PROVIDER`) takes over the same set of endpoints (status/unlock/lookup);
  switching back to local restores the semantics. Shape validation and auditing stay in the adapter layer and do not travel with the provider. Zero changes to business code.
- **② Touchpoint scan**: `./scripts/check_vault_touchpoints.sh` → `clean (boot-tier
  allowlist only)`; the CI `Vault Touchpoint Scan` job guards this continuously.
- **③ Tests green**: `cd orchestrator/server && go test -race -count=1 ./...` all green;
  commits in English and short; no tags, no releases.
