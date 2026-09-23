# 远程网关像素面集成 Apache Guacamole 设计

- 状态：已定案（设计），实现未启动
- 日期：2026-09-23
- 关联文档：`architecture-decisions.md`（硬约束）、`mobile-automation-design.md`（Mobile D1–D9 决策）、`ROADMAP.md`（A4 里程碑）
- 本文编号：**DG-x**（Guacamole 相关决策），与 Mobile D1–D9、架构 ADEC 并列互引

---

## 0. 一页总结

wingman 的远程能力分两个面：

| 面 | 载荷 | 协议 | 状态 |
|---|---|---|---|
| **控制面**（自动化） | 小 JSON 指令/事件/心跳 | 现有 Agent TCP（16 字节头 + JSON 体） | 已上线，**保持不动**（DG-1） |
| **像素面**（人看 + 人接管） | 高频二进制图形流 + 输入事件 | Guacamole 协议，经 guacd 翻译 RDP/VNC（DG-2） | 本文设计，未实现 |

像素面的平台覆盖（DG-3/DG-4/DG-5）：Windows 走 RDP（Pro 及以上；Home 版无 RDP host，VNC 兜底）、macOS 走内置屏幕共享（VNC/RFB）、Linux 走 x11vnc（优先）或 TigerVNC；Android 自动化数据面不推翻 MediaProjection 决策（DG-4），"人看 Android"以 droidVNC-NG → guacd VNC 桥为远期可选项单独评估（§6.1）；iOS 无公开屏幕共享 server API，明确不覆盖（DG-5）。

基建归属（DG-6）：guacd 网关、连接票据、审计日志归属 Go server，与 cockpit 项目**共用同一套**；两方 dashboard 前端都用 `guacamole-common-js` 渲染，谁先实现谁抽公共组件。

约束重申：runtime 在像素面里**零新增监听面**（不引入任何 HTTP/WebSocket server）；浏览器只连 Go server；全部新协议流量都收敛在 Go server 边界。

---

## 1. 背景与动机：为什么需要"像素面"

### 1.1 现有控制面能做什么、不能做什么

今天 wingman 的远程链路是：dashboard → Go server → Agent TCP → runtime。这条链上跑的是结构化指令——截图单帧（`screen.capture`）、点击/按键（`input.*`）、脚本任务下发与回执。它覆盖的是**自动化**：机器按脚本领办事。

但有一类需求它结构性覆盖不了：**人要看、人要临时上手**。

- 自动化脚本卡在一个预料外的弹窗/验证码/风控页，脚本作者需要**实时看设备画面**定位，而不是反复拉单帧截图拼猜。
- 长尾故障需要**人工接管**：人直接用键鼠在目标机操作几步，把状态拨回自动化能继续的位置，再交还脚本。
- 培训/演示场景：多人旁观一台正在被自动化驱动的机器。

这些场景的共同载荷是**连续画面 + 双向输入**，不是结构化指令。用控制面硬扛（比如 200ms 轮询全屏截图经 JSON base64 推给浏览器）在带宽、延迟、输入回传三个方向都是错误的工具选择。

### 1.2 为什么现在做

- Mobile D3 决策时已明确把 scrcpy 的"远程调试可视化（人看设备画面）"价值归入 A4 Dashboard 场景"另议"——本文就是那次"另议"的结论，并把结论从 Android 扩展到全平台。
- 架构上万事俱备：Go server 已有 WebSocket 边界（`orchestrator/server`，架构决策允许 dashboard↔Go server 使用 WebSocket）、RBAC 与审计骨架、agent 注册表；dashboard 是 React 18 + TS，集成渲染库是纯前端依赖。

---

## 2. 分层定案：控制面与像素面的协议边界

### DG-1：自动化控制面保留现有 Agent TCP（16 字节头 + JSON 体），不自研改道、不换行业协议栈

**为什么自研在这个面是合理的。** 控制面的载荷特征：消息小（几十字节到几 KB）、频率中低、语义是 wingman 领域的命令/事件/心跳、要求低延迟与断线重连。16 字节定长头 + JSON 体是一个**领域 RPC**协议：长度前缀解决粘包，JSON 换取全链路可调试性。它不是 HTTP，也不需要是——它服务的对象（runtime↔Go server）是一对固定端点之间的私有会话，没有通用互操作诉求。行业协议栈（HTTP/gRPC/WebSocket）在这个面的增益是"通用性"，而通用性恰恰是这里不需要的东西；代价（连接管理复杂度、头开销、runtime 侧引入 server 语义违背硬约束）却是实打实的。既存实现已在生产验证，重写只有风险没有收益。

**为什么这个面不接入 Guacamole。** Guacamole 是像素协议，不是控制协议。让它承载 JSON 命令等于用画面通道传文本——方向反了。

### DG-2：像素面（人看 + 接管）采用 Guacamole 协议 + guacd，不自研像素流

**为什么边界画在"载荷性质"上。** 协议选型跟着载荷走，而不是跟着"远近"或"新旧"走。控制面与像素面的差异不是"本地 vs 远程"（两者都远程），而是载荷的根本不同：

| | 控制面 | 像素面 |
|---|---|---|
| 载荷 | 离散小消息 | 连续视频帧 + 输入事件流 |
| 带宽 | KB/s | 0.5–10 Mbps（自适应编码） |
| 关键指标 | 可靠、有序 | 实时、自适应降质 |
| 技术难点 | 几乎没有（TCP 就解决） | 编码器选择、差量帧、输入合并、剪贴板同步、断线画面恢复 |

**为什么不自研像素流。** 表中右列的每一个难点都是 RDP（1996 起）和 VNC/RFB（1998 起）打磨了二十多年的领域；Guacamole 又把"这两个协议翻译成浏览器 canvas 指令流"这件事（2009 起）做成了 Apache 顶级项目。自研像素流 = 同时重做编码器自适应、协议状态机、输入语义映射三件事，且每一件都只在运行时才能暴露问题。用现成协议栈的这笔账没有任何悬念。

**为什么 Guacamole 而不是"浏览器直连 VNC"。** 浏览器没有原生 RFB/RDP 支持，任何方案都需要一个"翻译层 + WebSocket 出口"。Guacamole 生态把这一层产品化了：**guacd**（C 守护进程）负责协议翻译，前端 `guacamole-common-js` 负责渲染。我们要写的只剩"Go server 里的那座桥"（§4）。

一个值得点出的呼应：Guacamole 协议与 wingman Agent TCP 同属"长度前缀帧"家族（Guacamole 用 `len.` 前缀的指令，Agent TCP 用 16 字节头），团队心智模型一致，排查工具（tcpdump/strace 看帧）通用。

---

## 3. 备选方案对比（为什么不是它们）

| 方案 | 结论 | 为什么不 |
|---|---|---|
| 自研 MJPEG/截图轮询流 | 否 | 每帧全量 JPEG 无差量编码，带宽随分辨率爆炸；输入回传、剪贴板、断线恢复全要自造；本质是把 RFB 重新发明一遍且发明得更差 |
| WebRTC（自建 SFU/信令） | 否（第一性否决有保留，见下） | 延迟确实最优，但代价：DTLS/STUN/TURN 基建、信令服务器、SDP 协商全要建；企业内网部署复杂度陡增（wingman 的目标部署形态是客户内网中控，不是公网服务）；输入注入、剪贴板等仍无现成语义，只在"传画面"上比 Guacamole 好。**保留**：若未来出现公网弱网移动端监看的硬需求，WebRTC 是唯一候选，届时作为第二像素通道另立项，与 guacd 通道并存而非替换 |
| scrcpy 系（Android） | 否（已有决策） | Mobile D3 已论证：scrcpy server 需 adb 常连，违背 D1 端侧自治；其可视化价值归入本文像素面，Android 侧见 §6.1 |
| 完整 Apache Guacamole webapp（Java guacamole-client） | 否 | 那是一套带自己用户体系、连接管理 GUI、存储的完整产品——与 wingman/cockpit 已有的 RBAC、agent 注册表、审计**全面重复**。只要 guacd（无状态翻译器）+ 自研 Go 桥 + 前端渲染库三个最小件即可，重复基建一件不引入 |
| NoMachine/向日葵等商业远控 | 否 | 闭源、按席计费、无法嵌进自有 dashboard；数据链路不经我们控制，审计不可行 |

---

## 4. 总体架构与数据流

### 4.1 组件与流向

```
浏览器 dashboard (React, guacamole-common-js)
   │  WebSocket（Guacamole 指令流）——既有 Go server WS 边界
   ▼
Go server（orchestrator/server）
   │  ├─ gateway 桥：WS ↔ guacd TCP 透传 + select/connect 握手注入
   │  ├─ 票据：REST 申请短时效 viewing token → WS 升级时校验
   │  └─ 审计：连接建立/断开/时长/操作者/目标 agent（沿用 rbac 审计）
   │  TCP 4822（guacd 默认端口，guacd 仅 localhost 监听）
   ▼
guacd（无状态协议翻译守护进程，与 Go server 同机部署）
   │  RDP                        VNC/RFB                    VNC/RFB
   ▼                             ▼                          ▼
Windows endpoint            macOS endpoint             Linux endpoint
（内置 RDP host，            （系统设置开启              （x11vnc 附着真实
 Pro+；Home 版 VNC 兜底）     屏幕共享）                  X display；或 TigerVNC）
```

### 4.2 Go 桥在 guacd 握手中的角色

guacd 对外说 Guacamole 协议。官方 Java webapp 扮演的角色（握手发起方 + WebSocket 出口）由 Go server 的 gateway 模块接管：

1. dashboard 先向 Go server REST 申请票据（操作者身份、目标 agent、权限校验、审计"申请"事件）；
2. 浏览器以票据发起 WebSocket 升级，Go 桥校验后连 guacd，发送 `select` 指令（协议 `rdp` 或 `vnc`，由目标平台的 endpoint 矩阵决定）；
3. guacd 回 `args` 要求连接参数，Go 桥回 `connect` 指令，附 endpoint 地址、端口与凭证——**凭证只存在于 Go 桥→guacd 这一段内存里，永远不下发浏览器**；
4. guacd 连真实 endpoint 成功后，Go 桥进入纯透传模式：两侧字节流原样转发，生命周期结束（任一侧断开）即写审计"断开 + 时长"。

这个结构有两个直接红利：**审计天然完备**（每条连接的申请/建立/断开都经过桥，无旁路）；**endpoint 异构性被 guacd 吸收**（前端从头到尾只见 Guacamole 协议，不知道也不需要知道背后是 RDP 还是 RFB）。

### 4.3 runtime 的角色：零新增监听面

像素面里 runtime（agent 进程）**不在数据链路上**。VNC/RDP server 是 endpoint OS 的系统服务，guacd 直连它们，与 wingman runtime 无关。runtime 仅有的参与是既有能力上报机制的扩展：注册时上报 `remotePreview` 能力位（检测本机 RDP/VNC server 是否可达），供 dashboard 展示"该机可监看/不可监看"。这是既有 outbound 控制面上加一个字段，不新增任何监听端口、不引入任何 server 语义——架构决策的 Forbidden Changes 全部无触碰。

### 4.4 endpoint 地址的解析

Go server 需要知道目标机的 VNC/RDP 端点地址。来源：agent 注册/心跳上报的局域网地址（既有字段）+ 平台默认端口（RDP 3389、VNC 5900），允许 endpoint 矩阵里按 agent 配置覆盖。地址解析失败（agent 离线/端口不可达）在票据申请时就拒绝，不留给握手期。

---

## 5. 桌面平台 endpoint 矩阵（DG-3）

| 平台 | 首选 | 兜底 | 理由 |
|---|---|---|---|
| Windows | RDP host（Pro 及以上内置） | VNC（TightVNC/UltraVNC） | RDP 是 Windows 原生虚拟通道协议：H.264/RemoteFX 编码带宽效率远超 RFB，剪贴板/输入语义原生；**Home 版没有的是 RDP host 的授权而非技术**，故 VNC 兜底。RDP host 已随系统存在，零新增软件面 |
| macOS | 内置屏幕共享（VNC/RFB） | — | macOS"屏幕共享"即标准 RFB server，系统设置一开即用；第三方 VNC server 反而新增安装与权限面。RFB 的带宽劣势在 macOS 单机监看场景可接受 |
| Linux | x11vnc（附着真实 X display） | TigerVNC（独立 Xvnc 会话） | wingman 自动化跑在**真实桌面**（X11 平台层已覆盖），监看/接管必须看到同一个桌面——x11vnc 附着 `:0` 与自动化同屏；TigerVNC 起的是独立虚拟会话，看到的不是自动化那块屏，只在"远程开发桌"场景有意义，列为兜底 |

**为什么按平台差异选型而不是一刀切 VNC。** 一刀切确实实现最简（guacd 只配 vnc），但 Windows 上放弃了 RDP 的编码效率与系统内置（要求用户额外装 VNC server 是纯粹的体验倒退），Linux 上放弃了"同屏"语义。guacd 同时吃两种协议，前端零差异——平台差异在 endpoint 矩阵一次消化，不让任何一层感受它。

---

## 6. Android 与 iOS 边界

### DG-4：Android 自动化数据面不推翻——像素采集继续走 MediaProjection 链

Mobile D3 已裁决：自动化数据面用 MediaProjection 自采集，拒绝 scrcpy（adb 常连违背 D1 端侧自治）。本设计**不推翻也不触碰**该决策：Android 的自动化截图/采集路径保持现状，Guacamole 不进入 Android 自动化数据面。

### 6.1 "人看 Android"：droidVNC-NG → guacd VNC 桥的取舍（单独成节）

**动机。** 若希望 Android 也获得与桌面三平台一致的"人看 + 人管"体验，社区方案是 [droidVNC-NG](https://github.com/bk138/droidVNC-NG)：无 root 的 Android VNC server，以普通 App 形式运行，MediaProjection 采集画面、AccessibilityService 注入输入，监听本地端口供 guacd 以标准 VNC 接入。接通后 guacd 侧与桌面 VNC 完全同构。

**收益。**
- Android 像素面与桌面共用同一 guacd/桥/票据/审计基建，dashboard 渲染零差异；
- 采集不与 wingman 的 MediaProjection 链冲突（两次独立授权、两个独立 VirtualDisplay，系统层面并行合法）。

**成本与风险（为什么它不进第一批）。**
1. **检测面/部署面**：设备上新增一个第三方 App 及其监听端口。Mobile D4 拒绝 uiautomator2 server 的核心理由——设备上长驻第三方注入组件——在这里**同源成立**。droidVNC-NG 服务的是人工接管面而非自动化数据面，性质上有别于 D4 的场景，但风控敏感场景（wingman 的典型目标环境）对"多一个 App + 多一个端口"的暴露是真实成本。
2. **注入双写者**：它的输入注入走 AccessibilityService，与 wingman Android agent 的无障碍注入通道并存。接管语义要求定义竞争规则（人接管时自动化暂停？按键路由互斥？）——这是一块未设计的语义，不是接个线就完事。
3. **分发与维护**：第三方 App 的版本演进、厂商 ROM 的无障碍限制差异，都是长期维护面。

**结论**：列为**远期可选项**，触发条件是出现真实的"人管 Android"需求且接受上述成本；届时补独立设计（含注入仲裁语义）。第一批不实现、不部署、不依赖。

### DG-5：iOS 明确不覆盖

iOS 没有公开 API 允许第三方 App 提供 VNC/RFB/RDP server（Broadcast 上传扩展只出不进，且无任何注入通道）。这不是"暂缓"而是**平台不可能**，除非 Apple 开放屏幕共享 server 权限。文档记录在案，避免后续重复评估。

---

## 7. 与 cockpit 共享（DG-6）

cockpit 项目同样需要"浏览器看画面 + 接管"能力，且 wingman 与 cockpit 共用同一中控部署。拍板如下：

1. **guacd 网关、连接票据、审计日志：一套，不各写一遍。** guacd 与 Go server gateway 模块是唯一权威实现；cockpit 以客户端身份消费（同一套票据 REST + WS 端点约定）。归属：Go server（orchestrator 仓库）。
2. **前端渲染：双方 dashboard 都用 `guacamole-common-js`。** wingman dashboard（React 18 + TS）与 cockpit dashboard 各自集成，但连接管理逻辑抽公共件。
3. **谁先实现谁抽公共组件。** 抽取边界约定为四件：连接生命周期 hook（建连/重连/断连状态机）、票据获取客户端、错误与降级展示、监看/接管模式切换与工具栏。抽取时机：第一方（wingman 或 cockpit）实现完成后立即抽，不允许第二方复制粘贴——复制品会立刻在协议细节上分叉。

---

## 8. 安全模型

- **凭证隔离**：RDP/VNC 凭证只在 Go 桥 → guacd 的 `connect` 指令参数里存在，不进浏览器、不进 URL、不进日志（桥对指令流做脱敏后才可审计留存）。
- **票据**：短时效（默认 60s 一次性）viewing token；WS 升级时校验操作者 RBAC 权限（新增 `remote-view` / `remote-control` 两个权限点，接管比监看多一档）+ 目标 agent 在线 + endpoint 可达。
- **网络拓扑**：guacd 仅 localhost 监听（`guacd -b 127.0.0.1`），endpoint 的 VNC/RDP 端口只在内网可达，一律不经公网暴露；浏览器到像素面的唯一入口是 Go server 的 WS 边界。
- **审计**：票据申请、连接建立、断开（含时长）、接管/监看切换，全部落既有 RBAC 审计体系；审计记录含操作者、目标 agent、协议类型，不含凭证与画面内容。
- **传输加密**：第一期内网明文 WS + guacd 本地明文（与既有 dashboard↔Go server 的安全假设一致）；公网部署场景由既有的 Go server TLS 终结覆盖，guacd 段始终不出主机。

---

## 9. 硬约束合规核查表

对照 `architecture-decisions.md` Non-Negotiable Control Plane 与 Forbidden Changes：

| 硬约束 | 本设计 | 判定 |
|---|---|---|
| Go server 是远程中控编排器 | guacd 网关、票据、审计全部落在 Go server | ✅ |
| Runtime 作为 agent 主动 outbound 连接 Go server | runtime 像素面零参与，仅既有 outbound 上加能力字段 | ✅ |
| 本地 Tauri UI 通过本地 IPC 控制 runtime | 未触碰；本地 UI 不在本设计范围 | ✅ |
| **Runtime 禁止引入 HTTP/WebSocket server** | runtime 无任何新增监听；VNC/RDP server 是 OS 系统服务，与 runtime 进程无关 | ✅ |
| Dashboard/远程客户端只连接 Go server | 浏览器唯一入口 = Go server WS 边界；不直连 guacd、不直连 endpoint | ✅ |
| WebSocket 允许在 Go server 边界 | Guacamole 指令流跑在既有允许的 dashboard↔Go server WS 上 | ✅ |
| Mobile D3：自动化数据面拒 scrcpy | 不推翻；Android 像素面为远期可选项且走 VNC 桥而非 scrcpy | ✅ |
| Mobile D4：拒设备上长驻第三方注入 server（自动化数据面） | 自动化数据面不涉及；droidVNC-NG 属人工接管面、单独评估并列为远期 | ✅（边界重申于 §6.1） |
| 平台宏边界（`platform/<os>/`） | 像素面无 C++ 平台代码；平台差异全部在 endpoint 部署矩阵（OS 级服务），不产生新宏分支 | ✅ |

---

## 10. 风险与"不做"清单

**风险与对策**

| 风险 | 对策 |
|---|---|
| RFB 带宽高（尤其 Windows 兜底路径） | 兜底路径明示为"可用"而非"舒适"；文档写清 Pro 版 RDP 的体验差；guacd 自带 JPEG 压缩率/色深参数按 agent 配置降质 |
| guacd 单点 | guacd 无状态，崩溃重连即可恢复（桥检测断开、票据重申请）；不做 guacd 集群（当前规模不需要，先不加复杂度） |
| 接管与自动化的输入竞争（桌面平台） | 桌面平台 OS 层天然共享真实键鼠（人动即真动），语义自洽无需仲裁；Android 若远期落地则必须先设计仲裁（§6.1） |
| guacamole-common-js 维护活跃度 | Apache 顶级项目、RDP/VNC 生态稳定，协议层变化风险低；渲染层封装在公共组件内（§7 第 3 条），替换成本被隔离 |

**不做清单（明确拒绝，防反复）**

- 不做 WebRTC 像素通道（触发条件见 §3 保留条目）；
- 不做自研 MJPEG 流；
- 不部署 Java guacamole-client webapp；
- 不让 guacd 暴露公网、不让浏览器直连 guacd；
- 不在 runtime 里加任何像素采集转发（桌面像素面与 runtime 无关；Android 像素面远期走 VNC 桥也不经 runtime）；
- 不做 iOS（平台不可能，§6 DG-5）。

---

## 11. 实施分期

- **P0（本文档批准后的第一批实现）**：Go server gateway 桥 + guacd 部署约定 + Windows(RDP)/macOS(VNC)/Linux(x11vnc) 三平台 endpoint 矩阵 + wingman dashboard 监看/接管组件 + RBAC 两权限点 + 审计四事件。
- **P1**：cockpit 接入同一网关；按 §7 第 3 条抽取前端公共组件；审计报表呈现。
- **远期（触发式）**：droidVNC-NG 桥独立设计（含注入仲裁）；公网弱网场景的 WebRTC 第二通道评估。

---

## 12. 对既有文档的修订点

实现启动时（P0）需同步：

1. `architecture-decisions.md`：Allowed WebSocket Usage 一节补记"Guacamole 指令流（dashboard↔Go server）"；Forbidden Changes 补一行"浏览器不得直连 guacd/endpoint"。
2. `ROADMAP.md` A4 里程碑：把"scrcpy 只读预览（可选）"改为指向本文的像素面方案。
3. `mobile-automation-design.md` §7 风险表"scrcpy 进自动化数据面——不做"行补交叉引用（远期 Android 像素面走 §6.1 的 VNC 桥路线，非 scrcpy）。
