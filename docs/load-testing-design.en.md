> [中文](load-testing-design.md) | **English**

# Wingman Tide-Scale Load Testing Architecture Design (Proposal)

Status: **Draft, pending owner sign-off** (this document changes no existing behavior; once signed off it lands by milestone, and `docs/architecture-decisions.md` and `docs/architecture.md` are updated in sync).

Related decisions: `docs/architecture-decisions.md` (four-layer model / Capability / Execution / automation primitive boundaries), `docs/protocols.md`, `docs/agent-token-auth-design.md`.

## 1. Positioning and Goals

Wingman's core positioning is game automation testing (UI-layer simulation of real players). Without **changing that existing positioning**, this design adds a second execution backend: protocol-layer tide-scale load testing (10k-scale VU per machine). The two goals share the same behavior modeling, orchestration, and control plane; they only diverge at the "execution engine".

**Goals**

- ≥ 5000 concurrent VU per single load-test machine (echo baseline; real game protocols float depending on codec overhead)
- Scenarios (player behavior models) authored from the same source as UI automation tests, and reusable
- Trustworthy metrics: quantiles of latency / RPS / error classification, aggregated at campaign level
- Consistent with the existing deployment shape: the worker is an outbound agent, with no new inbound listener whatsoever

**Non-goals**

- Do not rebuild the reporting and ecosystem of k6/locust (metric export is handed off; heavy reporting goes to external tools)
- Do not pursue density gains for UI automation (the UI backend keeps its 1~20 streams/machine real-client positioning)
- Do not do automatic orchestration of mixed scenarios in v1 (the combination of UI canary + protocol tide relies on the operator running the two in parallel manually in v1, see §9)

## 2. Relationship to Existing Architecture Decisions

| Existing decision | How this design aligns |
|---------|---------------|
| Four-layer model (control plane / Agent / Runtime / Core) | The loadgen worker is a new member of the Agent+Runtime layers; the plan/codec/metrics primitives go into `lib/wingman` (Automation Core does not know the campaign concept) |
| Control plane has a single entry point / agent outbound | The worker reuses the agentcore registration/heartbeat/command channel, zero listeners |
| Dashboard connects only to the Go server | Metrics are reported through the agent channel; the dashboard reads the server API/WS |
| Capability System (already landed in v1) | New `loadgen.proto` / `loadgen.report` vocabulary; campaign scheduling writes `requires: {capabilities}` |
| Execution core object | Campaigns and worker deployments write Execution records (unified audit) |
| Automation primitive boundaries (Trigger<Script<BT<Workflow<Team) | A single VU behavior = a plan (Script/plan layer); cross-machine scale-out orchestration = **campaign (a sibling object of Workflow, both in the control plane)**, not stuffed into the workflow DAG engine (a campaign is a long-running job with telemetry, not a step graph) |
| Dual-language strategy (desktop Python-first) | Scenario authoring is Python-first; the codec declarative schema leaves no room for language disputes; the VU hot path has no interpreter |
| Platform macro boundary | IO multiplexing differences (epoll/kqueue/IOCP, smoothed over by asio) stay in `libs/transport` and `lib/wingman/src/platform/<os>/` |

## 3. Overall Layering

```text
CONTROL PLANE (orchestrator/)
  Dashboard · RBAC · Audit · Agent Registry · Workflow
  · LoadCampaign（新：campaign 生命周期 / worker 配额 / 指标聚合 / ramp 调度 / 熔断）
        ▲                            ▲
        │ agent TCP (outbound)       │ agent TCP (outbound)
AGENT LAYER                            │
  apps/agent（现有）                  apps/loadgen（新）
  UI 后端：screen/input/vision          VU 引擎：事件循环 × VU 状态机
  · 行为：fsm/task/orchestration        · plan 解释器（无脚本引擎）
        │                              · codec 运行时
        └────────────┬─────────────────┘
                     ▼
AUTOMATION CORE (lib/wingman + libs/*)
  新原语：scenario plan（schema）· codec 框架 · metrics 直方图
  复用：libs/transport（asio 会话）· libs/agentcore（注册/心跳）· task_core
```

Core idea: **write the behavior once, choose the backend for density**. The same game (gameprofile) + the same behavior vocabulary: the UI backend runs real clients, the protocol backend runs the tide.

## 4. Component Design

### 4.1 Scenario Plan (scenario plan) — the core contract of the two-stage execution model

The density constraint determines the execution shape: one Python interpreter per VU cannot hold up 10k-scale concurrency. A **two-stage authoring/execution** split is adopted:

1. **Authoring and validation** (script layer, Python-first): describe player behavior with the existing `task`/`fsm`/`orchestration` vocabulary; run a single VU / a few VUs to verify the logic is correct.
2. **Compilation into a plan** (declarative JSON): state machine + request templates + extraction/assertion + think-time distribution. The VU engine only interprets plans and embeds no script engine.

Plan schema (v1 shape):

```jsonc
{
  "planVersion": 1,
  "game": "mygame", "gameVersion": "1.4.2",
  "codec": "mygame@1.4.2",              // gameprofile 内 codec 引用（版本 pin）
  "session": { "connect": {...}, "handshake": ["C2S_Handshake", "C2S_Login"] },
  "vu": {
    "initial": "login",
    "states": {
      "login":    { "do": "login", "on": { "LoginOk": "lobby", "LoginFail": "exit" } },
      "lobby":    { "do": "enterRoom", "on": { "RoomOk": "fight" } },
      "fight":    { "do": "attackLoop", "on": { "BossDead": "loot", "VUNeedsHeal": "heal" } }
    }
  },
  "actions": {
    "login": {
      "send":  { "msg": "C2S_Login", "fields": { "user": "{{vu.id}}", "token": "{{vault:game.token}}" } },
      "await": { "msg": "S2C_LoginOk", "timeoutMs": 5000 },
      "extract": { "sessionId": "$.body.sessionId" },
      "thinkMs": { "dist": "lognormal", "median": 800, "sigma": 0.6 }
    }
  },
  "load": {
    "model": "closed",                   // closed=固定 VU 迭代 | open=到达率
    "vuCount": 5000,
    "ramp": [ { "toVU": 500, "seconds": 60 }, { "toVU": 5000, "seconds": 300 } ],
    "durationS": 1800,
    "stopOn": { "errorRatePct": 20, "p99Ms": 10000 }
  }
}
```

Expressiveness boundary (honest declaration): plans cover send/await/extract/assert/think/state transitions; logic beyond that boundary (unconventional encrypted handshakes, dynamic multi-packet negotiation) goes through the codec's native hooks (§4.2). **No Turing-complete scripting is introduced at the plan layer** — this is the across-the-board cut that protects density.

### 4.2 Codec Layer (gameprofile integration, four orthogonal pluggable layers)

Per-game protocol encode/decode is an ongoing cost (version drift, encryption, anti-replay). Design principle: **engine adaptation × encoding format × message schema × crypto hook — four orthogonal layers, hot-pluggable at deploy time**.

```jsonc
// codec bundle（gameprofile 内，纯数据；plan 的 "codec" 字段引用它）
{
  "engine": "kbe",            // kbe | bigworld | generic | custom:<name>
  "encoding": "protobuf",     // protobuf | json | sproto | raw
  "schema": "mygame@1.4.2",   // 消息表（id/字段布局/字节序/长度规则）
  "crypto": { "kind": "compositional", "primitives": ["rc4", "xor:0x5A"] }
  // 或 { "kind": "builtin", "name": "kbe-blowfish" }
  // 或 { "kind": "native", "name": "mygame-custom" }  // 编译期注册，罕见兜底
}
```

- **engine adapters** (session semantics such as framing / handshake / heartbeat / reconnect): `kbe` (KBEngine: MessageID + Bundle, dual loginapp/baseapp channels, built-in Blowfish link), `bigworld` (entity message / filtered packet families), `generic` (length prefix + single channel, covering most in-house protocols), `custom:<name>` (a compile-time-registered native adapter as a fallback). Adapters are compiled into the worker and selected per the bundle declaration — KBEngine/BigWorld are **protocol families**, not encoding formats, so built-in adapters are more faithful and less troublesome than a schema description.
- **encoding backends** (payload serialization, compile-time registry + build gating): `protobuf` (vcpkg protobuf), `json` (nlohmann, already in-repo), `sproto` (a standalone C library; **when vcpkg has no port, follow the dependency rule and fail loudly by disabling that tier — do not silently switch source**), `raw` (schema field table laid out directly).
- **schema message table**: pure data, pinned to the gameprofile version.
- **crypto**: prefer **compositional primitives** (xor / offset tables / rc4 / TEA family / checksums, declared inside the schema, covering most in-house variants, zero compilation); engine built-in encryption belongs to the engine adapter; only genuinely custom algorithms go through compile-time-registered native hooks.

**Hot-plug semantics (honest definition)**:

- ✅ **Hot-plug at deploy time**: a new game / new version = dropping a codec bundle (pure data + adapter selection) into the gameprofile, loaded when the worker is deployed, with **zero rebuilding of the engine binary**; switching protocols between campaigns does not touch the worker.
- ❌ **Switching protocols while a campaign is running**: not supported (it would pollute the load semantics); if you want to switch, stop → swap the bundle → re-arm.
- ❌ **`dlopen`-style C++ runtime plugins**: not done (C++ ABI maintenance cost > benefit, and it piles onto the Windows CI matrix); the pluggable unit is "bundle data selection + a compile-time registry", not a runtime dynamic library.

**Validation workflow** (the everyday loop for codec development):
  1. Capture traffic → turn it into a schema (tooling assistance, not automatically reverse-engineered within v1 scope)
  2. `proto.replay` single-connection replay A/B comparison (**semantic-level** parity with real client traffic; byte-level if golden traffic exists)
  3. Put the replay into CI (a single VU is cheap), so drift is intercepted at the codec layer instead of only surfacing once the tide runs

The script layer gains a new `proto` module (encode / decode / single-connection send-receive), serving codec development and single-connection validation, sharing the same codec runtime as the VU engine (a new primitive in `lib/wingman`).

### 4.3 VU Engine (apps/loadgen, new app)

- **Form**: `apps/loadgen` is a standalone binary (the third desktop app in the apps+lib architecture). Its threading model is thoroughly isolated from the runtime's screenshot injection path; it reuses `libs/agentcore` (registration/heartbeat/commands) and `libs/transport` (asio sessions).
- **Engine structure**:
  - Event loop: asio `io_context` sharded per core, with VUs sticky to a shard (avoiding cross-core migration)
  - VU = a lightweight state machine instance (current state + extracted variable table + timers), targeting < 4KB steady-state per instance
  - Connection lifecycle: connection pool + reconnect-storm backoff (exponential + jitter; protective slowdown when targets refuse, to avoid crushing the load-test machine itself)
  - Timer wheel: shared by think time / await timeouts
- **Command surface** (reusing the 16-byte header + JSON body protocol, Request/Response + sequence):
  `loadgen.deploy(planRef)` / `loadgen.start` / `loadgen.stop` / `loadgen.status`
- **No script engine is registered**: the plan interpreter is the only execution form; the `proto` module validation loop runs on the runtime side, not inside loadgen.

### 4.4 Metrics Surface

- **Worker side**: per-operation log-bucket histograms (HDR style) + counters + VU state gauges; local aggregation over a 1s window.
- **Reporting**: reuses `Notify agent.event`, `{event: "loadgen.metrics"}`, with the payload being compact JSON (bucket count arrays; within frame limits, estimated < 64KB/window; a hook is reserved to upgrade to a binary Notify subtype later, without touching the frame header).
- **Server side** (new package `internal/loadgen`): cross-worker merging (histogram additivity), in-memory sliding window state + campaign snapshots persisted to the DB (hanging off the same persistence as Execution); the dashboard uses the existing WS push stream.
- **Query semantics**: p50/p90/p95/p99/max, RPS, error classification per operation (protocol error / timeout / assertion failure / network disconnect), per-worker breakdown.
- **Export egress** (optional): one Prometheus remote-write endpoint; the ecosystem is left to external tools.

### 4.5 Control Plane LoadCampaign (Go server)

New package `internal/loadgen`; a campaign is a **sibling object** of Workflow (both are control-plane long-running jobs, but the DAG engine is not reused — a campaign is a scheduling job with telemetry):

```jsonc
{
  "campaignId": "lc-42",
  "name": "mygame 容量标定 5k",
  "scenario": { "planRef": "plans/mygame-5k.json", "codec": "mygame@1.4.2" },
  "workers": { "count": 4, "requires": { "capabilities": ["loadgen.proto"] } },
  "sharding": "vuRange",                 // worker0: VU 0-1249, ...
  "lifecycle": "draft → armed → running → stopping → done | aborted"
}
```

- **Scheduling**: capability matching (reusing the matcher of workflow `selectAgent`), VU range sharding, globally advanced ramp.
- **Clock**: v1 estimates skew from heartbeat RTT (±100ms is enough for ramp granularity); no NTP dependency is introduced.
- **Safety valve**: global stop (end-to-end ≤ 1s), error-rate/quantile circuit breaker (automatic load shedding or stopping after it trips), worker disconnection = its VU range is marked lost (no automatic backfill in v1).
- **Permissions**: a new `loadtests:run` RBAC domain; campaign operations are audited in full (hanging off executionId).
- **Mixed scenarios** (UI canary + protocol tide): v1 = the operator starts the campaign and the UI runtime's workflow from the dashboard at the same time; combined orchestration is left to the §9 decision.

### 4.6 Script-Layer Surface

- New module `proto` (codec development / single-connection validation): `proto.connect` / `proto.call` / `proto.decode` / `proto.replay`.
- The plan authoring loop: in v1 the plan lands directly in the gameprofile as JSON; the "export plan from script/fsm" compiler is a later enhancement and does not block M1.
- Dual language as before: ModuleDescriptor written once, with Lua/Python dual bindings + `.pyi`.

## 5. Constraint Compliance Checklist (against architecture-decisions.md)

- Runtime/loadgen: **zero new inbound listeners**; no runtime HTTP/WebSocket server.
- Dashboard/remote clients connect only to the Go server.
- The agent channel protocol is unchanged: 16-byte header + JSON body, sequence echo, bounded frames; the only additions are method names and Notify event types.
- Automation Core never gains the campaign/worker concept; plan/codec/metrics are pure primitives.
- Platform macros stay in the platform layer and in libs/transport.
- New third-party dependencies go through vcpkg (asio is already on the transport usage chain; protobuf goes through vcpkg protobuf; **when sproto has no port, the tier is explicitly disabled with an error — no silent source switch**, dependency rule #5).

## 6. Coordination with the Three Reserved Batches (order unchanged)

The orchestration batches already reserved in Todo are the shared foundation for both goals and land first:

| Reserved batch | How the load-testing side benefits |
|-------|-----------|
| Subtask aggregation | The orchestration layer for aggregating multi-worker campaign results is structurally identical |
| Process-level state events | The campaign/worker lifecycle event streams are structurally identical |
| Persistence | Campaign definitions/snapshots and codec version pins are persisted |

## 7. Milestones and Acceptance Criteria

| Milestone | Content | Acceptance |
|-------|------|------|
| M0 | The three reserved batches (already in the queue) | Existing criteria |
| M1 | plan schema + codec framework + `proto` module + single-VU replay | Semantic-level parity with real client traffic; byte-level parity with golden traffic; single-connection replay permanently in CI |
| M2 | VU engine + local metrics | echo-stub baseline **≥5k VU/worker** (RSS ≤ 2GB, idle event loop p99 ≤ 5ms); histogram parity against wrk (same scenario RPS ±5%, p99 deviation ≤ 10%) |
| M3 | LoadCampaign + multi-worker + dashboard view | Global stop ≤ 1s; merge correctness (sum of worker counters == server aggregate); ramp start-point deviation ≤ 100ms |
| M4 | Open model / circuit breaking / export / mixed drills | Circuit-breaker automatic-load-shedding drill passes; Prometheus export (if signed off); canary mixed-drill report |

Every milestone follows the existing iron rules: core_tests + the Python tree fully green, platform boundary guards, Windows CI compiles, short English commits.

## 8. Risks and Hedging

| Risk | Hedge |
|------|------|
| codec fidelity drift (client upgrades) | codec version pin + single-connection replay CI + centralized gameprofile management |
| Anti-cheat identifying protocol bots | Isolated environment / intranet deployment shape; `loadtests` environment label enters RBAC |
| Scope loss of control (rebuilding the k6 ecosystem) | The non-goals list is fixed; minimal metric surface + an export egress |
| Metric frames exceeding limits | 1s window + bucket count ceiling + estimated < 64KB; the over-limit path = binary subtype upgrade |
| The Python density trap recurring | No Turing-complete scripting at the plan layer is a hard boundary (§4.1) |
| ramp clock drift | The v1 ±100ms convention is written into acceptance; add clock synchronization only if that is insufficient |

## 9. Decision Points Left to the Owner by This Design

1. **apps/loadgen as a standalone agent process** (recommended by this design; the owner leaned toward a standalone agent on 2026-10-09 — the reasoning is on the three fronts of density / fault domain / deployment shape in §4.3; pending final confirmation to be recorded as a formal decision).
2. The density target number (this design proposes ≥5k VU/worker; it affects M2 acceptance).
3. The first batch of target games and codec priorities (this determines the actual pace after M1).
4. Whether Prometheus remote-write is needed in v1.
5. When automatic orchestration of mixed scenarios (UI canary × campaign) should be opened as an item (currently left as manual parallel operation).
6. Whether campaign snapshots hang off `models.Execution` or get their own `models.LoadCampaign` table (a separate table + audit hanging off executionId is recommended, to avoid bloating the Execution semantics).

## 10. Documentation Landing Requirements

Within the same change after sign-off: this file's status is changed to landed; `docs/architecture-decisions.md` gains a "Load Testing Backend" section (four-layer model diagram update, capability vocabulary, the boundary between campaign and Workflow); `docs/architecture.md` and `docs/protocols.md` add the `loadgen.*` commands and Notify events; the ROADMAP gains the load-testing milestones.