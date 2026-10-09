# Wingman 潮水级压测架构设计（提案）

状态：**草案，待 owner 拍板**（本文不改变任何现有行为；拍板后按里程碑落地，并同步 `docs/architecture-decisions.md` 与 `docs/architecture.md`）。

关联决策：`docs/architecture-decisions.md`（四层模型 / Capability / Execution / 自动化原语边界）、`docs/protocols.md`、`docs/agent-token-auth-design.md`。

## 1. 定位与目标

Wingman 的核心定位是游戏自动化测试（UI 层模拟真实玩家）。本设计在**不改变现有定位**的前提下，增加第二执行后端：协议层潮水级压测（万级 VU/机）。两个目标共享同一套行为建模、编排与控制面，只在"执行引擎"分家。

**目标**

- 单压测机 ≥ 5000 并发 VU（echo 基准；真实游戏协议视 codec 开销浮动）
- 场景（玩家行为模型）与 UI 自动化测试同源创作、可复用
- 指标可信：分位数延迟 / RPS / 错误分类，campaign 级聚合
- 与既有部署形态一致：worker 是 outbound agent，不新增任何入站监听

**非目标**

- 不重造 k6/locust 的报表与生态（指标导出外抛，重报告交给外部）
- 不做 UI 自动化的密度提升（UI 后端保持 1~20 路/机的真实客户端定位）
- 不在 v1 做混合场景的自动编排（UI 金丝雀 + 协议潮水的组合 v1 靠操作员手动并行，见 §9）

## 2. 与现有架构决策的关系

| 现有决策 | 本设计如何对齐 |
|---------|---------------|
| 四层模型（控制面/Agent/Runtime/Core） | loadgen worker = Agent+Runtime 层新成员；plan/codec/metrics 原语进 `lib/wingman`（Automation Core 不知道 campaign 概念） |
| 控制面唯一入口 / agent outbound | worker 复用 agentcore 注册/心跳/命令通道，零 listener |
| Dashboard 只连 Go server | 指标经 agent 通道上报，dashboard 读 server API/WS |
| Capability System（v1 已落地） | 新增 `loadgen.proto` / `loadgen.report` 词汇；campaign 调度写 `requires: {capabilities}` |
| Execution 核心对象 | campaign 与 worker 部署写 Execution 记录（审计统一） |
| 自动化原语边界（Trigger<Script<BT<Workflow<Team） | 单 VU 行为 = plan（Script/plan 层）；跨机放量编排 = **campaign（Workflow 的兄弟对象，同属控制面）**，不塞进 workflow DAG 引擎（campaign 是带遥测的长时作业，不是步骤图） |
| 双语言策略（桌面 Python-first） | 场景创作 Python-first；codec 声明式 schema 无语言之争；VU 热路径无解释器 |
| 平台宏边界 | IO 多路复用差异（epoll/kqueue/IOCP，经 asio 抹平）留在 `libs/transport` 与 `lib/wingman/src/platform/<os>/` |

## 3. 总体分层

```text
CONTROL PLANE (orchestrator/)
  Dashboard · RBAC · Audit · Agent Registry · Workflow
  · LoadCampaign（新：campaign 生命周期 / worker 配额 / 指标聚合 / ramp 调度 / 熔断）
        ▲                            ▲
        │ agent TCP (outbound)       │ agent TCP (outbound)
AGENT LAYER                            │
  apps/runtime（现有）                  apps/loadgen（新）
  UI 后端：screen/input/vision          VU 引擎：事件循环 × VU 状态机
  · 行为：fsm/task/orchestration        · plan 解释器（无脚本引擎）
        │                              · codec 运行时
        └────────────┬─────────────────┘
                     ▼
AUTOMATION CORE (lib/wingman + libs/*)
  新原语：scenario plan（schema）· codec 框架 · metrics 直方图
  复用：libs/transport（asio 会话）· libs/agentcore（注册/心跳）· task_core
```

核心思想：**行为写一遍，密度选后端**。同一个游戏（gameprofile）+ 同一套行为词汇，UI 后端跑真实客户端，协议后端跑潮水。

## 4. 组件设计

### 4.1 Scenario Plan（场景计划）——两段式执行的核心契约

密度约束决定执行形态：Python 每_VU 一个解释器撑不起万级并发。采用**创作/执行两段式**：

1. **创作与验证**（脚本层，Python-first）：用既有 `task`/`fsm`/`orchestration` 词汇描述玩家行为；跑单 VU/少量 VU 验证逻辑正确。
2. **编译为 plan**（声明式 JSON）：状态机 + 请求模板 + 提取/断言 + 思考时间分布。VU 引擎只解释 plan，不嵌脚本引擎。

Plan schema（v1 形状）：

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

表达力边界（诚实声明）：plan 覆盖 send/await/extract/assert/think/状态迁移；超出生界逻辑（非常规加密握手、动态多包协商）走 codec 的 native 钩子（§4.2），**不在 plan 层引入图灵完备脚本**——这是保密度的一刀切。

### 4.2 Codec 层（gameprofile 集成，四层正交可插拔）

每游戏协议编解码是持续成本（版本漂移、加密、反重放）。设计原则：**引擎适配 × 编码格式 × 消息 schema × 加密钩子四层正交，部署期热插拔**。

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

- **engine 适配器**（分帧/握手/心跳/重连等会话语义）：`kbe`（KBEngine：MessageID + Bundle、loginapp/baseapp 双通道、内置 Blowfish 链路）、`bigworld`（entity message / filtered packet 家族）、`generic`（长度前缀 + 单通道，覆盖多数自研协议）、`custom:<name>`（编译期注册的原生适配器兜底）。适配器编译进 worker、按 bundle 声明选择——KBEngine/BigWorld 是**协议族**不是编码格式，内置适配器比 schema 描述更保真、更省事。
- **encoding 编码后端**（payload 序列化，编译期注册表 + 构建门控）：`protobuf`（vcpkg protobuf）、`json`（nlohmann，仓内已有）、`sproto`（独立 C 库；**vcpkg 无 port 时按依赖规则显式报错关档，不静默换源**）、`raw`（schema 字段表直排）。
- **schema 消息表**：纯数据，随 gameprofile 版本 pin。
- **crypto**：优先**组合式原语**（xor/偏移表/rc4/TEA 族/校验和，schema 内声明，覆盖多数自研变体，零编译）；引擎内置加密归 engine 适配器；真自定义算法才走编译期注册的 native 钩子。

**热插拔语义（诚实定义）**：

- ✅ **部署期热插拔**：新游戏/新版本 = 往 gameprofile 投一个 codec bundle（纯数据 + 适配器选择），worker 部署时加载，**引擎二进制零重建**；campaign 之间换协议不动 worker。
- ❌ **campaign 运行中换协议**：不支持（会污染负载语义）；要换就 stop → 换 bundle → 重新 arm。
- ❌ **dlopen 式 C++ 运行时插件**：不做（C++ ABI 维护成本 > 收益，Windows CI 矩阵雪上加霜）；可插拔单位是"bundle 数据选择 + 编译期注册表"，不是运行时动态库。

**验证工作流**（codec 开发的日常回路）：
  1. 抓包 → schema 化（工具侧辅助，不在 v1 范围内自动反推）
  2. `proto.replay` 单连接回放对拍（与真实客户端流量**语义级一致**；有 golden 流量则字节级）
  3. 回放进 CI（单 VU 便宜），漂移在 codec 层被拦截，不进潮水才暴露

脚本层新增 `proto` 模块（编码/解码/单连接收发），服务 codec 开发与单连接验证，与 VU 引擎共用同一套 codec 运行时（`lib/wingman` 新原语）。

### 4.3 VU 引擎（apps/loadgen，新 app）

- **形态**：`apps/loadgen` 独立二进制（apps+lib 架构的第三桌面 app）。线程模型与 runtime 的截图注入链路彻底隔离；复用 `libs/agentcore`（注册/心跳/命令）与 `libs/transport`（asio 会话）。
- **引擎结构**：
  - 事件循环：asio `io_context` per-core 分片，VU 按分片粘滞（避免跨核迁移）
  - VU = 轻状态机实例（当前状态 + 提取变量表 + 计时器），目标单实例 < 4KB 稳态
  - 连接生命周期：连接池 + 重连风暴退避（指数 + 抖动；目标拒绝时保护性降速，避免压死压测机自身）
  - 定时器轮：think time / await 超时共用
- **命令面**（沿用 16 字节头 + JSON body 协议，Request/Response + sequence）：
  `loadgen.deploy(planRef)` / `loadgen.start` / `loadgen.stop` / `loadgen.status`
- **不注册脚本引擎**：plan 解释器是唯一执行形态；`proto` 模块验证回路跑在 runtime 侧，不在 loadgen 里。

### 4.4 指标面

- **worker 侧**：每操作 log-bucket 直方图（HDR 风格）+ 计数器 + VU 状态量规；1s 窗口本地聚合。
- **上报**：复用 `Notify agent.event`，`{event: "loadgen.metrics"}`，负载 = 紧凑 JSON（桶计数数组；帧界内，预估 < 64KB/窗口；预留升级为二进制 Notify 子类型的口子，不动帧头）。
- **server 侧**（`internal/loadgen` 新包）：跨 worker 合并（直方图可加性）、滑动窗口内存态 + campaign 快照落库（挂 Execution 同款持久化）；dashboard 走既有 WS 推流。
- **查询语义**：p50/p90/p95/p99/max、RPS、按操作错误分类（协议错误 / 超时 / 断言失败 / 网络断开）、per-worker 分解。
- **外抛出口**（可选）：Prometheus remote-write 一个端点，生态交给外部。

### 4.5 控制面 LoadCampaign（Go server）

`internal/loadgen` 新包，campaign 是 Workflow 的**兄弟对象**（同属控制面长时作业，但不复用 DAG 引擎——campaign 是带遥测的调度作业）：

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

- **调度**：capability 匹配（复用 workflow `selectAgent` 的匹配器）、VU 区间分片、ramp 全局推进。
- **时钟**：v1 用心跳 RTT 估算偏移（±100ms 足够 ramp 粒度）；不引入 NTP 依赖。
- **安全阀**：全局 stop（端到端 ≤ 1s）、错误率/分位数熔断（触发后自动降载或停止）、worker 失联 = 其 VU 区间标记 lost（不自动补位，v1）。
- **权限**：新 `loadtests:run` RBAC 域；campaign 操作全量审计（挂 executionId）。
- **混合场景**（UI 金丝雀 + 协议潮水）：v1 = 操作员在 dashboard 同时发起 campaign 与 UI runtime 的 workflow；组合编排留 §9 决策。

### 4.6 脚本层开放面

- 新模块 `proto`（codec 开发/单连接验证）：`proto.connect` / `proto.call` / `proto.decode` / `proto.replay`。
- plan 的创作回路：v1 plan 直接以 JSON 落 gameprofile；「从脚本/fsm 导出 plan」的编译器是后续增强，不阻塞 M1。
- 双语言照旧：ModuleDescriptor 一次编写，Lua/Python 双绑定 + `.pyi`。

## 5. 约束合规清单（对照 architecture-decisions.md）

- Runtime/loadgen **零新增入站监听**；无 runtime HTTP/WebSocket server。
- Dashboard/远程客户端只连 Go server。
- agent 通道协议不变：16 字节头 + JSON body、sequence 回显、有界帧；新增的只是 method 名与 Notify 事件类型。
- Automation Core 不出现 campaign/worker 概念；plan/codec/metrics 是纯原语。
- 平台宏留在 platform 层与 libs/transport。
- 新三方依赖走 vcpkg（asio 已在 transport 使用链上；protobuf 走 vcpkg protobuf；**sproto 无 port 时显式关档报错**，不静默换源——依赖规则第 5 条）。

## 6. 与三留批次的衔接（顺序不变）

Todo 中已留的编排批次是两目标共用地基，先落：

| 留批次 | 压测侧受益 |
|-------|-----------|
| 子任务聚合 | campaign 多 worker 结果聚合的编排层同构 |
| 流程级状态事件 | campaign/worker 生命周期事件流同构 |
| 持久化 | campaign 定义/快照、codec 版本 pin 落库 |

## 7. 里程碑与验收口径

| 里程碑 | 内容 | 验收 |
|-------|------|------|
| M0 | 三留批次（已在队列） | 既有口径 |
| M1 | plan schema + codec 框架 + `proto` 模块 + 单 VU 回放 | 与真实客户端流量语义级对拍；golden 流量字节级一致；CI 单连接回放常驻 |
| M2 | VU 引擎 + 本地指标 | echo-stub 基准 **≥5k VU/worker**（RSS ≤ 2GB、空载事件环 p99 ≤ 5ms）；直方图对拍 wrk（同场景 RPS ±5%、p99 偏差 ≤ 10%） |
| M3 | LoadCampaign + 多 worker + dashboard 视图 | 全局 stop ≤ 1s；合并正确性（worker 计数器之和 == server 聚合）；ramp 起点偏差 ≤ 100ms |
| M4 | 开放模型/熔断/导出/混合演练 | 熔断自动降载演练通过；Prometheus 导出（若拍板）；金丝雀混合演练报告 |

每个里程碑照既有铁律：core_tests + python 树全绿、平台边界守卫、Windows CI 编译通过、英文短 commit。

## 8. 风险与对冲

| 风险 | 对冲 |
|------|------|
| codec 保真度漂移（客户端升级） | codec 版本 pin + 单连接回放 CI + gameprofile 集中管理 |
| 反作弊识别协议 bot | 隔离环境/内网部署形态；`loadtests` 环境标签进 RBAC |
| 范围失控（重造 k6 生态） | 非目标清单写死；指标最小面 + 外抛出口 |
| 指标帧超界 | 1s 窗口 + 桶数上限 + 预估 < 64KB；超界路径 = 二进制子类型升级 |
| Python 密度陷阱复发 | plan 层无图灵完备脚本是硬边界（§4.1） |
| ramp 时钟漂移 | v1 ±100ms 口径写进验收；不够再上校时 |

## 9. 本设计留给 owner 的决策点

1. **apps/loadgen 独立 agent 进程**（本设计推荐，owner 2026-10-09 倾向独立 agent——理由见 §4.3 的密度/故障域/部署形态三面；待最终确认收编为正式决策）。
2. 密度目标数字（本设计按 ≥5k VU/worker 提案；影响 M2 验收）。
3. 首批目标游戏与 codec 优先级（决定 M1 之后的实际节奏）。
4. Prometheus remote-write 是否 v1 就要。
5. 混合场景（UI 金丝雀 × campaign）的自动编排何时立项（当前留在手动并行）。
6. campaign 快照挂 `models.Execution` 还是独立 `models.LoadCampaign` 表（推荐独立表 + audit 挂 executionId，避免 Execution 语义膨胀）。

## 10. 文档落地要求

拍板后同一变更内：本文件状态改 landed；`docs/architecture-decisions.md` 增「Load Testing Backend」节（四层模型图更新、capability 词汇、campaign 与 Workflow 的边界）；`docs/architecture.md` 与 `docs/protocols.md` 补 `loadgen.*` 命令与 Notify 事件；ROADMAP 增压测里程碑。
