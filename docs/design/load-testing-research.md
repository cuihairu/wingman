# 游戏压测场景扩展调研：协议加载机制、目标形态与分期

> 分析日期：2026-10-08。
> 回答的问题：wingman 能否扩展为「自动化测试 + 压测框架」，最终成为分布式的压测框架？
> 范围：调研与方案设计，不含实现。
> 调研范围更正：gzoltar 是 Java 覆盖率工具、packettracer 是网络模拟器，均不属压测参照；本文将「pcap 类工具」按 tcpreplay 系（流量回放）取证。

---

## 1. 结论速览

| 问题 | 结论 |
|------|------|
| 做不做 | **做**。现有资产（outbound agent 链路、脚本引擎、batch 分发）恰好是分布式压测的三层骨架，缺口集中在 codec/调度器/指标管道 |
| 分几期 | **四期**：期 0 PoC（约 1 周，零新依赖）→ 期 1 单机框架化（2-3 周）→ 期 2 server 任务分发+指标汇聚（3-4 周）→ 期 3 分布式调度完善（4-6 周）。期 0+1 完成即有单机可用的压测能力 |
| 先做什么协议 | **wingman 自有 16 字节头 + JSON 帧（Agent TCP 协议）**。两端编解码现成、Lua `string.pack` 可直接构造帧、压测自家控制面即得真实基准；第二步 gRPC（vcpkg manifest 引入 + 服务端反射），KBEngine entitydef 视目标游戏再排期 |
| 最硬的一条口径 | 分布式下分位数必须**合桶重算、禁止跨节点取平均**（Locust 的做法，见 §4.3） |

---

## 2. 调研：加载协议做压测的机制

### 2.1 KBEngine：entitydef 协议事实源 + bots 压测组件

KBEngine 是「游戏引擎自带压测方案」的完整样本，答案可以概括为一句话：**压测端与协议事实源同体**。

**协议定义与线上格式**

- 消息在各组件 `*_interface.h` 用 C++ 宏声明，MessageID（uint16）按注册顺序自动分配；`messages_fixed_defaults.xml` 可把指定消息钉死 ID——这是给第三方客户端做协议表对接留的口子，也是协议演进的手段。
- 线上格式（官方 kbe_message_format 文档 + `bundle.cpp`/`packet_reader.cpp` 取证）：定长消息 `msgID(u16)+body`；变长 `msgID(u16)+len(u16)+body`；长度 ≥65535 时 uint16 段填 0xFFFF 转义、后跟 uint32 真实长度。包上限 TCP 1460 / UDP 1472（MTU 对齐）；加密仅 Blowfish，密钥由客户端生成、hello 握手时上送。
- **entitydef 是协议单一事实源**：`scripts/entity_defs/*.def`（XML）声明 Properties（含同步域 Flags）、Base/Cell/ClientMethods（含参数类型）、Utype（手工指定 uint16 协议 ID）；`entities.xml` 声明实体分部；`types.xml` 放自定义类型。属性/方法 ID、参数编解码全部由 def 推导。
- 客户端 RPC **零代码生成**：引擎内客户端/bots 启动时解析同一份 def 运行时绑定 Python 实体类；第三方客户端有两条路——loginapp/baseapp 暴露 `importClientMessages`/`importClientEntityDef` 运行时下发协议表（**这是 KBEngine 版的「服务端反射」**，与 gRPC reflection 同构），或 `kbcmd --clientsdk` 离线生成 Unity/UE SDK。
- **协议一致性门禁**：服务端对全部消息表与 entitydef 各算一份 MD5 digest，hello 握手回传，登录时不一致即 `SERVER_ERR_ENTITYDEFS_NOT_MATCH` 拒绝。含义：改协议必须两端同批重建重启，无跨版本协商、def 无热更。

**bots 组件（官方模拟客户端压测）**

- 位置 `kbe/src/server/tools/bots/`，与 cellapp 平级的独立进程，直接复用 `client_lib` 整个客户端协议栈——协议 100% 同构，AOI/属性同步全额生效。一个 bots 进程 = N 个虚拟客户端，单进程单事件循环。
- 数量与爬坡：xml `defaultAddBots` 配 `totalCount/tickTime/tickCount`（总数/间隔秒/每批数量），运行时 `KBEngine.addBots(n, tick, time)` 动态加压；账号前缀+递增后缀自动生成。容量口径：官方 stresstest README 建议 **~50 bot/进程、多机多进程**（demo xml 写 1000/进程，两处冲突，取 README；单进程实际上限官方无公开数据）。
- 会话模拟：显式七态状态机 INIT→CREATE→LOGIN→BASEAPP_CREATE→BASEAPP→PLAY→DESTROYED，每步有错误码回调；心跳按「通道超时一半」周期发 activeTick；断线重连回 INIT 重走。
- 行为建模：bot 加载 `scripts/bots/` 下的实体 Python 类，在 def 声明的 ClientMethods 回调里推进业务（demo `Account.py`：建号→选人→进游戏全链回调驱动）；官方 stresstest demo 的负载模型是随机游走/找怪攻击/传送三模式每 60s 轮换。
- 指标：每消息 send/recv 计数内嵌 MessageHandler + 全局包/字节计数 + Watcher 树（GUIConsole 四档 profiler、`KBEngine.addWatcher` 自定义指标）+ telnet REPL。**主仓没有时延分位/TPS 报表**——分位报表、多机聚合、压测机饱和诊断（`owned_cpu_p95_max_percent` 容量阈值，区分「引擎长尾」与「压测机饱和」）都是社区分支 KBEngine-Nex（2026 活跃）补的；官方 stresstest 万人 demo 工程 2019 年后停更。另澄清：`kbcmd` 是 SDK 生成器，不是压测工具，社区教程常误列。

**对 wingman 的可移植机制**：变长转义帧（uint16→uint32）、def→协议摘要的版本门禁、每消息内嵌计数、半超时心跳、钉 ID 的协议演进、行为模式定时轮换。其中「协议摘要门禁」对 wingman 有直接参照价值——16B+JSON 帧协议目前没有版本协商字段，扩压测时若引入协议演进，可抄 digest 校验的做法。

### 2.2 gRPC 反射与 protobuf 自描述

gRPC 生态把「加载协议」做到了三通道，按依赖从重到轻：

1. **proto 文件**：`--proto` 指定 `.proto` 源文件，本地编译；
2. **protoset**：`protoc --descriptor_set_out=bundle.protoset *.proto` 生成编译好的描述符集合，压测端只带这一个文件，不需要编译环境；
3. **Server Reflection**：没给前两者时 ghz 自动走服务端反射。服务端启用成本一行：`reflection.Register(s)`（grpc-go）。反射返回 `FileDescriptorProto`，gRPC 官方文档明确其中包含「all transitive dependencies of the returned file」——即全部传递依赖的完整 schema。

gRPC 官方对 reflection 的定位是调试工具（"an optional extension for servers to assist clients in runtime construction of requests"），官方协议文档没有安全章节。对压测这是双刃：**schema 自动发现意味着零协议准备工作**，但等于把服务的完整接口清单暴露给任何能连上端口的客户端，生产环境开不开属于暴露面决策。

protobuf 消息本身还有一条自描述路径（`Any` 类型带 type URL + 服务端 type registry），但工程上实际通行的是上面三通道——ghz 的选项面即业界共识形状。

### 2.3 自定义帧协议：pcap 回放为什么不够

tcpreplay 系是「不解析协议、直接回放字节」的极端形态，官方 FAQ 自己划了界：

- "Other Tcpreplay products don't understand the state of common protocols like TCP."——主链路不理解 TCP 状态；
- "is unable to synchronize Syn/Ack's to create valid TCP sessions"——不握手、不应答、不跟随对端序列号；
- 因此正确靶标是**旁路/串联的无状态网络设备**（IDS/防火墙/流统计），不是应用服务器。

唯一真会话特例 `tcpliveplay` 会自己完成三次握手并跟随服务器响应，但硬限制是 "only works with single-session TCP captures where the client side is what you're replaying"——一条 pcap 只回放一条会话，没有并发模型、没有速率-响应闭环，是正确性验证工具而非压测工具。

结论：**压测游戏服务端这类有状态应用，必须是应用层模拟器**（握手/请求响应配对/登录态/业务流编排），pcap 回放给不了失败率与响应时间指标。成熟框架（JMeter/Locust/k6/ghz）全部走这条路。

### 2.4 接入方式小结

| 协议形态 | 最省力的接入 | 依据 |
|----------|--------------|------|
| 有 IDL（proto/FlatBuffers） | schema 驱动自动派生（protoset / 反射），解析代码近零 | ghz 三通道 |
| 引擎自有 def/接口定义（KBEngine 类） | 压测端复用引擎协议栈（bots 模式），或运行时协议表下发对接 | §2.1 |
| wingman 自有帧（16B 头 + JSON） | 两端编解码现成 + Lua `string.pack` 构造二进制头 | 见 §5.1 |
| 无文档二进制协议 | 逆向 + 手写 codec，属另一个问题域 | —— |
| 纯抓包样本、靶标是网络设备 | pcap 回放（tcpreplay） | §2.3 |

---

## 3. 复杂度分级：六层拆解

把「加载 RPC/protobuf 协议做压测」拆成六层，每层给定级与依据（依据来自 §4 各框架的官方机制）：

| 层 | 定级 | 成熟做法 | wingman 现状（对照 §5） |
|----|------|----------|--------------------------|
| L1 协议解析 | 轻-中 | 有 IDL 走 schema 派生（近零）；手写帧协议抽一个 codec 层，用户只写帧读写 | 16B 帧只存在于 C++ 内部，脚本层无帧读写器；Lua `string.pack` 可绕过 |
| L2 流量生成 | 轻 | 开环（到达率）/闭环（并发数）两族，k6 六种 executor + ghz `--load-schedule` 是选项面共识 | 每脚本一线程 + `timer.every` ≈ 闭环雏形；无开环调度器 |
| L3 会话模拟 | 轻-重 | 用户=协程/线程内顺序脚本 + 连接复用；业务状态机永远自己写 | 脚本层 TCP **无接收路径**（`setMessageHandler` 只打 debug 日志），UDP 有同步 `udpRecvFrom` |
| L4 断言 | 轻 | 样本级断言 + 门禁级阈值两层，照抄 k6 thresholds 表达式形状 | 无 |
| L5 分布式调度 | 中-重 | Locust 消息协议（9 种上行 + 8 种下行）是最完整开源参考；降级方案是 k6 execution-segment 手切 | batch.go 有并发分发但语义是「全量下发」非负载切分（同 JMeter 语义，见 §4.2） |
| L6 指标汇聚报表 | 轻-中 | 分桶上报 + 合桶重算分位（Locust 已完全公开该算法）；RPS/失败率可加直接求和 | server 侧仅 4 个 Prometheus 指标，无请求级延迟管道 |

**总评**：六层没有一层是「不可能」级。最重的 L5 可以先降级为 segment 切分拿结果，L6 的算法已被 Locust 公开化，真正要新写的是 L1 的 codec 抽象与 L3 的接收路径。

---

## 4. 成熟框架横向参照

### 4.1 五个框架对比

| 维度 | JMeter | Locust | k6 | ghz | tcpreplay 系 |
|------|--------|--------|----|----|--------------|
| 协议解析成本 | 中（Java 插件） | 轻（Python client 类 + 事件上报） | 中-重（Go 扩展 + 私有二进制） | 零（gRPC 内，三通道） | 零（不解析） |
| 会话模拟力 | 中-强（线程=用户 + 变量关联） | 强（纯代码写业务流） | 中-强（VU 生命周期 + checks） | 弱（请求级） | 弱（无状态） |
| 分布式 | 成熟但「每节点跑全量」，瓶颈在 master 汇聚 | **最成熟**（ZMQ + 心跳/重连/配额/桶化上报） | 单机优先，OSS 分布式 "not entirely functional" | 无 | 无 |
| 指标报表 | 强（HTML dashboard，分位口径可配） | 中（桶化重算，合并口径正确） | 中-强（thresholds 门禁 + 退出码） | 中（直方图 + 分位 + 错误分布） | 弱 |
| 自定义协议接入 | 重（官方自述 "isn't necessarily easy"，参考插件耗两周业余时间） | 轻-中（库需 gevent 可 monkey-patch） | 重（xk6 build 维护私有二进制） | 不适用 | 轻（有 pcap 时） |

关键量化数字（官方口径）：

- JMeter 单节点 1000-2000 线程，分布式语义 "JMeter does not distribute the load between servers, each runs the full test plan"——1000 线程 × 6 节点 = 6000 线程；
- Locust 官方基准：单核约 16k RPS（FastHttpUser）/ 4k RPS（HttpUser）；瓶颈在 RPS/CPU 不在用户数——"thousands or even tens of thousands of Users per process just fine, as long as their total request rate (RPS) is not too high"；
- k6 单实例 30-40k VU、300k RPS，官方建议 "Unless you need more than 100,000-300,000 requests per second … a single instance of k6 is likely sufficient"。

### 4.2 分布式架构三种形态

1. **全量计划复制**（JMeter）：每节点跑完整计划，负载 = 节点数 × 线程数。实现简单但容量规划反直觉，master 汇聚是瓶颈（2.9 起默认 StrippedBatch 剥离响应体缓解）。
2. **master/worker 负载切分**（Locust）：master 不发压，只派用户配额（`spawn` 消息）+ 汇聚统计；worker 心跳（双向 `heartbeat`）、掉线广播 `reconnect`、握手 `client_ready`/`ack`。通道 ZeroMQ ROUTER/DEALER + msgpack，默认端口 5557。消息类型表（worker→master 9 种、master→worker 8 种）来自源码取证，见参考资料。
3. **无中心、手工切段**（k6）：`--execution-segment` 按迭代序号切负载，thresholds 按实例各自评估，指标外部汇聚。官方承认 OSS 分布式 "not entirely functional"，生产路径是 k6-operator 或云版。

### 4.3 指标汇聚的硬口径

分布式压测唯一容易做错的地方是**分位数**：跨节点对分位取平均是错的（p95 的平均不是 p95）。Locust 的标准答案（源码 `stats.py` 取证）：

- worker 侧响应时间按约 2 位有效数字分桶（147→150、3432→3400），"This limits the dict to ~310 unique keys, which is important for bandwidth in distributed mode"——分桶本身就是带宽优化；
- master 收到 `stats` 消息后 `StatsEntry.extend()` 逐 key 累加桶，**合桶后重算全局分位**，min/max 传精确值；
- RPS/失败率是可加量，按秒桶相加即可：`fail_ratio = num_failures / num_requests`。

k6 的 thresholds 形状（`'http_req_duration{tag}': ['p(95)<500']` + `abortOnFail` + 退出码）是门禁层值得照抄的表达式格式；JMeter dashboard 的分位滑动窗口估计器（`statistic_window` 默认 20000）是单机报表的参照。

---

## 5. wingman 现状：可复用资产与缺口

### 5.1 可复用资产

按「压测框架三层」（负载生成端 / 任务分发层 / 指标汇聚层）盘点，出处均为仓内源码：

**传输与协议**

- Agent TCP 帧：16 字节头 `{uint32 length; uint32 sequence; uint8 type; 3B padding; uint32 reserved}` + JSON 体，`MessageHeader` 定义在 `libs/transport/include/wingman/transport/session/session.hpp:36-41`，Go 侧互证 `orchestrator/server/internal/agent/client.go:18`。注意头是 `memcpy` 整结构体、**host 字节序**，Go 侧按宿主机字节序探测编解码（`listener.go:158-166`）——跨机构造帧时这是 L1 的隐性约束。
- 脚本层流量能力现成：`transport_module.cpp` 导出 TCP 10 个函数（`tcpConnect/tcpSend/tcpDisconnect/...`）+ UDP 5 个函数（`udpSocket/udpBind/udpSendTo/udpRecvFrom/udpClose`）。**UDP 是请求-响应可用状态**（`udpRecvFrom` 带超时同步收）；**TCP 无脚本可见的接收路径**（`setMessageHandler` 只打 debug 日志，`transport_module.cpp:284-289`），这是 L3 的第一块补丁。
- 另有 SimpleProtocol（4 字节大端长度前缀，16MiB 上限）与三流分离（CONTROL/SCREEN/EVENT 各自独立 socket、独立参数）——压控制面走 CONTROL 流端口。

**负载执行端（agent 侧）**

- 每脚本一线程：`ScriptManager::runScriptInternal` 为每个运行脚本起独立 `std::thread`（`lib/wingman/src/script_manager.cpp:353-357`）——多脚本并发即多线程，天然的「脚本=虚拟用户」底座。
- `timer.after/every/setTimeout/setInterval`（`timer_module.cpp:167-246`）提供周期驱动；Python 引擎受 GIL 约束（`python_script_engine.cpp:172` 起各入口 acquire），**负载生成建议用 Lua**。
- RemoteClient 出站长连接：指数退避 5s→60s + 抖动、心跳 30s、断线出站缓冲（`remote_client.hpp:129-136`）——agent 永远是发起方，压测目标地址可经 `run_script` 参数下发，agent 出站连 SUT 属于既有出站模型，不碰「runtime 禁监听」红线。

**任务分发层（server 侧）**

- batch fan-out：`internal/handlers/batch.go` 选择器（agentIds+tags）+ goroutine 信号量（`maxBatchConcurrency = 8`，`:22`）+ 每 agent 独立超时 + `BatchSummary` 汇总——控制面批量下发的完整样板，但**语义是全量下发非负载切分**。
- workflow 引擎：DAG 依赖调度、环检测、step 超时/重试/负载均衡（`internal/workflow/engine.go`）、按 capabilities 选 agent——能承载压测任务的依赖编排；无 cron 定时触发，只能 API 触发。
- registry：注册/心跳/离线判定（30s ticker、90s 超时）/tags/capabilities（`internal/agent/registry.go`）——压测节点管理现成。
- teams/inbox：agent 间消息通道（`internal/agent/team.go`）——worker 间协调可借用，但压测的 spawn/stats 流量走这里属于复用边界内新增语义。

**指标汇聚层**

- server 侧已有 Prometheus 端点但仅 4 个指标：`wingman_http_requests_total`、`wingman_active_websocket_connections`、`wingman_registered_agents`、`wingman_uptime_seconds`（`handlers/metrics.go:15-37`）——HTTP 面，压测核心指标（请求级延迟/分位/RPS）一条都没有。
- 事件上行通道现成：`EventBuffer`（`event_buffer.hpp:36-60`）→ `agent.event` Notify → server 广播 Dashboard WS——但为脚本输出设计（drain 上限 500），压测统计若逐请求走此通道会打爆 16MiB 帧上限，必须先 agent 侧分桶再上报。
- remote gateway（Guacamole）验证了「agent TCP 链路上开字节流隧道」的先例（`guac_relay.go`，`proxy.data` + `GuacRelayPrefix="guac:"`）——压测指标/大流量回传有现成的代理协议参照。

### 5.2 缺口（对照六层）

1. **L1 codec 层缺失**：16B 帧格式没有暴露给脚本的帧读写器；protobuf/gRPC 依赖未入 `vcpkg.json`（历史上 `protobuf/` 目录 2026-09-22 已移除，从未接入链路——引入是新决策，不是恢复旧目录）。
2. **L2 无调度器**：有 `timer.every` 但没有开环到达率、ramp 曲线、速率上限。
3. **L3 接收路径断点**：脚本 TCP 只发不收；请求-响应配对、超时管理要从零写。
4. **L4 断言与门禁为零**：没有样本断言，没有阈值→退出码的 CI 门禁。
5. **L5 无负载切分语义**：batch 是全量下发；没有 Locust 式配额分配与 worker 心跳。
6. **L6 指标管道空白**：请求级计数/延迟分桶/合桶重算一条都没有；现成 4 指标全是控制面健康度。
7. **报表与门禁产物**：无 end-of-test 汇总、无 dashboard 压测页。

---

## 6. 目标形态评估与分期

### 6.1 两级目标形态

- **形态 A（单机压测框架）**：wingman 单实例内，脚本即虚拟用户，codec + 调度器 + 断言 + 本地报表。这是「自动化测试 + 压测」合一的第一步——同一套脚本 API，断言跑通是测试、开并发是压测。
- **形态 B（分布式压测框架）**：Go server 任务分发（压测任务→按配额切给 agent 节点）→ agent 出站连 SUT 打压 → 统计分桶上报 → server 合桶重算 + 报表。节点复用 registry/batch，分发语义从「全量」升级为「切分」，指标管道从零新建。

### 6.2 架构约束（不可触碰）

- Runtime 只出站不监听：负载生成走 agent 出站连接（既有 `RemoteClient` 同款模型），禁止为压测给 runtime 加 TCP 监听（`architecture-decisions.md` Forbidden Changes）。
- 控制面数据只走 agent TCP / 本地 IPC：压测任务下发与指标回传复用 16B 帧链路，Dashboard 永远只连 Go server。
- 依赖统一走 vcpkg：protobuf/grpc 若引入，进 `vcpkg.json` manifest + toolchain，不走系统库探测（CLAUDE.md 依赖管理规则）。
- 报表数字诚实：写入文档的性能数字一律注明测量条件（产品文案用词规范「能力描述只写可验证事实」）。

### 6.3 分期路线与工作量

| 期 | 内容 | 工作量 | 出口判据 |
|----|------|--------|----------|
| 期 0 PoC | Lua `string.pack` 构造 16B 帧 + 现有 `transport` 模块连自家 Go server 打 `run_script`/`get_status`；`timer.every` 开 N 脚本并发；脚本内手写计时统计 | 约 1 周，零新依赖 | 拿到控制面的 RPS/延迟第一组真实数字，验证「脚本=虚拟用户」模型 |
| 期 1 单机框架化 | ① codec 层（16B 帧读写器暴露给脚本 + protobuf codec，依赖入 vcpkg manifest）；② 负载调度器（闭环并发 + 开环到达率 + ramp）；③ 样本断言 + thresholds 门禁（照 k6 形状，失败→退出码）；④ 本地报表（Locust 式分桶 + 分位重算）；⑤ 脚本 TCP 接收路径补丁 | 2-3 周 | 一条 Lua/Python 压测脚本 + 一条命令出报告与门禁结论 |
| 期 2 server 分发与汇聚 | ① 压测任务 API（batch.go 扩展出「配额切分」语义，参照 workflow 的超时/重试记账）；② 指标上行（agent 侧分桶后走 `agent.event` 或专用 Notify，帧上限约束）；③ server 合桶重算 + Prometheus histogram；④ Dashboard 压测报告页 | 3-4 周 | N 台 agent 一键下发压测任务，报告页出全局分位 |
| 期 3 分布式完善 | ① Locust 式 worker 消息协议（spawn/heartbeat/stats/reconnect 全表，见 §4.2）；② worker 动态加入/退出与配额再平衡；③ gRPC codec + 服务端反射自动发现；④（视目标游戏）KBEngine entitydef codec | 4-6 周 | 多节点自动扩缩、按 Locust 消息表收敛的调度闭环 |

合计约 10-14 周单人量级；期 0+1 完成即具备单机压测能力，期 2 完成即具备「用现有 agent 机群做分布式压测」的最小可用形态。

---

## 7. 拍板建议

1. **做**，但按上面四期切，不一次全上。理由：六层拆解无「不可能」级，最重的 L5 有降级方案（先 segment 切分），最贵的 L6 算法已被 Locust 公开化；而现有资产覆盖了负载执行端（脚本线程模型）、分发层（batch/workflow/registry）两层骨架，新建的只有 codec/调度器/指标管道三块。
2. **期 0 立即启动**：零新依赖、约一周，直接用现有 `transport` 模块压自家控制面——既是 PoC 又是给 Go server 的性能基线，这组数字本身就有工程价值。
3. **先做 wingman 自有 16B+JSON 协议**：两端编解码现成（`session.hpp` + Go `client.go` 互证）、Lua `string.pack` 可直接构造帧头、不引入任何 IDL 依赖；压自家控制面还顺带验证 agent 链路容量。第二优先 gRPC（vcpkg 引入 grpc + 反射自动发现，ghz 三通道是选项面参照）。KBEngine 系目标排期 期 3，且路径不是手写 codec——KBEngine 自己的答案是压测端复用引擎协议栈（bots 模式）或对接 `importClientMessages` 运行时协议表下发（KBEngine 版反射，见 §2.1），wingman 侧做 codec 的性价比远低于做这两条对接。
4. **分布式那一步等期 1 出真数字再定规模**：单机性能上限直接决定要切多少节点。若期 1 实测单机已达目标量级（k6 单实例 30-40k VU 的参照），期 3 可以降级甚至不做。

---

## 8. 参考资料

**KBEngine**

- 源码取证：[kbengine/kbengine](https://github.com/kbengine/kbengine)——`kbe/src/lib/network/{bundle.cpp,packet_reader.cpp,message_handler.cpp,fixed_messages.h,common.h,encryption_filter.h}`（帧格式/ID 分配/钉 ID/加密）、`kbe/src/lib/entitydef/entitydef.cpp`（def 加载与 MD5）、`kbe/src/server/loginapp/loginapp.cpp`（hello digest 校验、importClientMessages）、`kbe/src/server/tools/bots/`（bots 全部机制）
- 官方文档：[消息线上格式](https://kbengine.github.io/docs/programming/kbe_message_format.html)、[entitydef 格式与同步域](https://kbengine.github.io/docs/programming/entitydef.html)、[固定消息 ID 配置](https://kbengine.github.io/docs/configuration/messages_fixed_defaults.html)、[Stress Test](https://kbengine.github.io/docs/documentations/stresstest.html)、[在线调试（profile/watcher）](https://kbengine.github.io/docs/documentations/onlinedebugging.html)
- 压测工程：[kbengine_stresstest（万人 demo，2019 停更）](https://github.com/kbengine/kbengine_stresstest)、[KBEngineLab 镜像（2026 更新）](https://github.com/KBEngineLab/kbengine_stresstest)、[bot 行为脚本样例（kbengine_demos_assets）](https://github.com/kbengine/kbengine_demos_assets)
- 社区分支 KBEngine-Nex（分位报表/多机聚合/压测机饱和诊断）：[releases](https://github.com/KBEngineLab/KBEngine-Nex/releases)、[DbStress 插件](https://github.com/KBEngineLab/KBEngineNex-Plugin-DbStress)、[压测方法论](https://www.kbelab.com/tutorial/manual/engine-intro.html)
- 未取证项：bots 单进程实际上限（官方无公开数字，README ~50/进程与 demo xml 1000/进程口径冲突）；KCP 在 bots 中的默认启用面

**gRPC / protobuf**

- [gRPC Server Reflection 协议](https://github.com/grpc/grpc/blob/master/doc/server-reflection.md)
- [grpc-go Server Reflection 教程（`reflection.Register(s)` 一行启用）](https://github.com/grpc/grpc-go/blob/master/Documentation/server-reflection-tutorial.md)
- [ghz options 文档（proto/protoset/reflection 三通道）](https://ghz.sh/)、[ghz README](https://github.com/bojand/ghz)、[ghz output 文档](https://ghz.sh/docs/output)、[load schedule 文档](https://ghz.sh/docs/load)

**pcap 回放**

- [tcpreplay 官方仓库](https://github.com/appneta/tcpreplay)、[FAQ（TCP 状态局限原话）](https://tcpreplay.appneta.com/docs/faq.html)、[tcpliveplay 文档（单会话限制）](https://tcpreplay.appneta.com/docs/tools/tcpliveplay.html)

**JMeter**

- [Test Plan 手册](https://jmeter.apache.org/usermanual/test_plan.html)、[分布式测试](https://jmeter.apache.org/usermanual/jmeter_distributed_testing_step_by_step.html)、[Remote Testing（全量计划语义）](https://jmeter.apache.org/usermanual/remote-test.html)、[Dashboard 报告生成](https://jmeter.apache.org/usermanual/generating-dashboard.html)、[TCP Sampler](https://jmeter.apache.org/usermanual/component_reference.html#TCP_Sampler)、[Java Request 插件 Javadoc](https://jmeter.apache.org/api/org/apache/jmeter/protocol/java/sampler/AbstractJavaSamplerClient.html)

**Locust**

- [What is Locust](https://docs.locust.io/en/stable/what-is-locust.html)、[Running distributed](https://docs.locust.io/en/stable/running-distributed.html)、[Increase performance](https://docs.locust.io/en/stable/increase-performance.html)、[Testing other systems（协议扩展模式）](https://docs.locust.io/en/stable/testing-other-systems.html)
- 源码取证：[rpc/zmqrpc.py](https://github.com/locustio/locust/blob/master/locust/rpc/zmqrpc.py)、[rpc/protocol.py](https://github.com/locustio/locust/blob/master/locust/rpc/protocol.py)、[runners.py（消息类型表）](https://github.com/locustio/locust/blob/master/locust/runners.py)、[stats.py（分桶与合桶算法）](https://github.com/locustio/locust/blob/master/locust/stats.py)

**k6**

- [Scenarios/Executors](https://grafana.com/docs/k6/latest/using-k6/scenarios/executors/)、[Thresholds](https://grafana.com/docs/k6/latest/using-k6/thresholds/)、[Running large tests（单实例口径）](https://grafana.com/docs/k6/latest/testing-guides/running-large-tests/)、[Running distributed tests](https://grafana.com/docs/k6/latest/testing-guides/running-distributed-tests/)、[Extensions（xk6）](https://grafana.com/docs/k6/latest/extensions/)

**仓内文档**

- [protocols.md](../protocols.md)：三条链路协议规范（16B 帧布局、心跳/超时、鉴权）
- [architecture-decisions.md](../architecture-decisions.md)：控制面硬约束与 Forbidden Changes
- [remote-gateway-guacamole-design.md](../remote-gateway-guacamole-design.md)：agent 链路字节流隧道先例
- [mobile-support-feasibility.md](../mobile-support-feasibility.md)：云控模式可行性分析（分级与拍板的文档范式）
