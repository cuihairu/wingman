# 远程网关像素面集成 Apache Guacamole 设计

- 状态：P0 已实现（服务端网关 + 票据 + RBAC + 三协议 e2e + 前端组件，2026-09-23）；阶段二（剪贴板控制 UI、文件传输、会话录制）2026-09-25 实现完成（设计见 §14–§16，DG-7/DG-8/DG-9）；P1（cockpit 接入、前端公共组件抽取、审计报表）未启动
- 日期：2026-09-23（P0 设计），2026-09-25（阶段二设计）
- 关联文档：`architecture-decisions.md`（硬约束）、`mobile-automation-design.md`（Mobile D1–D9 决策）、`ROADMAP.md`（A4 里程碑）
- 本文编号：**DG-x**（Guacamole 相关决策），与 Mobile D1–D9、架构 ADEC 并列互引
- 实现落点：`orchestrator/server/internal/handlers/guacamole.go`（网关）、
  `internal/remoteticket/`（一次性票据）、`deployments/guacd/`（部署，锁定 1.5.5）、
  `orchestrator/dashboard/src/services/remote.ts` + `src/components/RemoteDesktopModal/`（前端）；
  e2e：`orchestrator/server/integration/guacd_e2e_test.go`（SSH/VNC/RDP 三协议真实链路）

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
| 任意（SSH server） | SSH（guacd ssh 插件，终端） | — | 不是"看桌面"而是"开终端"：guacd 在服务端跑终端仿真并把终端画面作为像素流下发，前端与桌面会话同构（P0 已含，e2e 三协议之一）。运维场景（重启服务/查日志）不需要整桌面，SSH 是最小暴露面。与桌面三行正交：一台机器可同时有桌面（VNC/RDP）与终端（SSH）两条像素面路径 |

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

### 7.1 抽取落地（2026-09-25，wingman 侧先行）

按第 3 条四件边界抽出公共件 `orchestrator/dashboard/src/components/RemoteDesktop/`，
wingman 侧入口 `index.ts` 即第二方的接入点（cockpit 只需 import 同一出口，
不 fork 组件）：

| 边界件 | 实现 | 关键约束 |
|---|---|---|
| 连接生命周期 hook | `useGuacamoleSession.ts` | 票据一次性 → **无自动重连**（重连即重新申请票据）；卸载/参数变化必 disconnect + 清空 stage；监看不挂输入面 |
| 票据获取客户端 | `types.ts` 的 `TicketClient` 接口 + `createWingmanTicketClient()` | 抽成接口而非直接调本仓 `services/remote`：两边 API base/鉴权可不同 |
| 错误与降级展示 | `RemoteErrorNotice.tsx` | 权限/未配置**不渲染重试**（重试无意义且诱导反复点），断链/未知才给重试 |
| 监看接管与工具栏 | `RemoteDesktopToolbar.tsx` | 监看 = 无输入注入 + 无发送入口（双层约束）；VNC 整块文件 UI 不渲染 |

配套：`RemoteDesktopPanel.tsx` 是容器无关的合成件（画布 + 工具栏 + 错误条），
`RemoteDesktopModal` 降级为 Modal 容器适配器（wingman Agents 页用），cockpit
可放进抽屉/全屏页。类型单一来源：`RemoteProtocol`/`RemoteSessionParams` 由
`types.ts` 唯一定义，`services/remote.ts` 不再重复声明（避免协议枚举分叉——
正是第 3 条要防的那类复制分叉）。

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

- **P0（本文档批准后的第一批实现）**：Go server gateway 桥 + guacd 部署约定 + Windows(RDP)/macOS(VNC)/Linux(x11vnc) 三平台 endpoint 矩阵 + wingman dashboard 监看/接管组件 + RBAC 两权限点 + 审计四事件。✅ 2026-09-23 完成。
- **阶段二（2026-09-25 实现）**：剪贴板控制 UI（§14，DG-7）+ 文件传输（§15，DG-8）+ 会话录制与检索（§16，DG-9）。
- **P1**：cockpit 接入同一网关；按 §7 第 3 条抽取前端公共组件（✅ 2026-09-25 wingman 侧先行，见 §7.1）；审计报表呈现。
- **远期（触发式）**：droidVNC-NG 桥独立设计（含注入仲裁）；公网弱网场景的 WebRTC 第二通道评估；SSH/SFTP 文件浏览器 UI（guacd `filesystem` 对象已可用，第一版只做拖拽上行 + 被动下行，见 §15「不做」）。

---

## 12. 对既有文档的修订点

实现启动时（P0）需同步（✅ 三项均已于 2026-09-23 随 P0 实现完成）：

1. ✅ `architecture-decisions.md`：Allowed WebSocket Usage 一节补记"Guacamole 指令流（dashboard↔Go server）"；Forbidden Changes 补一行"浏览器不得直连 guacd/endpoint"。
2. ✅ A4 里程碑行（实际位于 `mobile-automation-design.md` §6，ROADMAP.md 无移动端条目）：把"scrcpy 只读预览（可选）"改为指向本文的像素面方案。
3. ✅ `mobile-automation-design.md` §7 风险表"scrcpy 进自动化数据面——不做"行补交叉引用（远期 Android 像素面走 §6.1 的 VNC 桥路线，非 scrcpy）。

---

## 13. P0 实现验证备注（2026-09-23，三协议 e2e 实测）

### 13.1 Guacamole 协议要点（实现固化的坑）

- **connect 参数必须与 args 名单按位置一一对应且等长**：guacd 对 select 返回
  args 指令（首段为协议版本名如 `VERSION_1_5_0`），connect 必须按名单顺序
  逐位填值（未提供参数空串占位），且**首参必须回应该版本串**。guacd 对参数
  个数硬校验——个数不等直接静默断连（`Client did not return the expected
  number of arguments`），浏览器侧只表现为会话无响应。回归护栏：
  `TestGuacApplyVersionArg`（39 段真实 SSH 名单对齐断言）。
- **隧道内部指令**（空 opcode，如 ping）由网关拦截回显，绝不转发 guacd
  （common-js 15s receiveTimeout 语义）。
- 票据双通道：URL query `?ticket=`（首选，common-js WebSocketTunnel 硬编码
  subprotocol 且把 connect(data) 拼 query）+ `Sec-WebSocket-Protocol[0]` 兼容位。

### 13.2 guacd 版本锁定：1.5.5（1.6.0 双崩溃）

官方 1.6.0 镜像（含 2026-02 latest 重建）RDP 链路 gdb 实测两个空指针崩溃，
均无客户端侧规避参数：

1. `guac_audio_assign_encoder`（libguac 未链接音频编码器，RDPSND 协商即崩）
   ——网关已对 1.5.5 显式 `disable-audio` 规避同类路径；
2. `guac_user_supports_webp`（1.6.0 display 重构后 WebP 能力探测竞态崩溃，
   无 connect 参数可关）。

锁定 1.5.5 直至上游修复；`deployments/guacd/` 与 e2e compose 均已固化。
另：`GUACD_LOG_LEVEL=debug` 会打印 connect 参数（含口令），生产保持 info。

### 13.3 对 cockpit 的反哺项（P1 抽取前核对）

cockpit 现网关实现（`internal/server/api_guacamole.go`）的 connect 以
`name=value` 形式发参数，与协议要求的按位对应不符（guacd 会把
`hostname=10.0.0.5` 整串当 host 值）——与本文 §13.1 同坑，联调时需先对齐；
wingman 版实现与单测护栏可直接复用（DG-6：第一方实现完成后抽公共组件，
不允许第二方复制粘贴分叉）。

---

## 14. 阶段二：剪贴板控制 UI（DG-7）

### 选型与理由

剪贴板同步用 Guacamole 协议原生 `clipboard` 指令（双向）：guacd 各协议插件
已实现 RDP 剪贴板通道 / VNC(RFB) Clipboard CutText / SSH 终端内剪贴板（经
终端转义序列）的翻译，前端用 `guacamole-common-js` 的 `onclipboard` 事件
（收）与 `createClipboardStream` + blob 流（发）。**网关零改动**——
`clipboard`/`blob`/`ack` 指令走 P0 已有的双向透传管道，网关不解析内容
（数据面纯管道原则不破例）。

### 协议与数据流

```
下行（远端 → 面板）：远端复制 → guacd 发 clipboard 指令（分流入站）
  → common-js onclipboard(stream, mimetype) → 逐 blob 收集（每 blob 必须
  ack，否则 guacd 停发）→ onend → base64 解码 → 面板文本框展示
  → 用户点「复制到本机」才写浏览器剪贴板（navigator.clipboard，用户手势）
上行（面板 → 远端）：面板输入文本 → createClipboardStream("text/plain")
  → sendBlob(base64(UTF-8)) → sendEnd → guacd 写远端剪贴板
```

### 与既有链路衔接

- 票据/RBAC/审计零变化（剪贴板是已授权会话内的数据，不单独设权限点）；
- 监看（read-only）模式隐藏「发送到远端」入口：guacd 的 read-only 参数
  拦的是输入注入，剪贴板写入是否被各插件同等对待存在协议间差异，UI 层
  不给入口是最保守且无歧义的约束（网关不解析指令内容，故不在网关拦）；
- 只处理 `text/plain`（mimetype 过滤）：文件型剪贴板（图片/文件列表）
  与 §15 文件通道语义重叠，第一版不接。

### 备选与否决

| 方案 | 否决理由 |
|---|---|
| 网关解析并中转剪贴板内容 | 破坏数据面纯管道原则；网关从此要理解流语义（ack 编排、分片重组），复杂度全部转移进 Go 且无收益 |
| 自动同步到本机剪贴板（免点击） | 浏览器写剪贴板需用户手势与权限；自动写等于让远端机器静默改本机剪贴板，嗅探面过大 |
| 仅保留单向下行（只看不发） | RDP/VNC 双向都是现成的，砍上行没有省任何东西，接管场景（粘贴命令到远端终端）恰是高频需求 |

---

## 15. 阶段二：文件传输（DG-8）

### 选型与理由

文件通道用各后端协议的原生文件语义，全部由 guacd 翻译为 Guacamole 协议的
`file`/`blob`/`ack` 流：

| 协议 | 通道 | connect 参数 | 语义 |
|---|---|---|---|
| SSH | SFTP 子系统 | `enable-sftp=true`（恒开） | 上行=拖拽文件经 SFTP 写到目标机用户家目录等默认路径；guacd 同时上报 `filesystem` 对象（供后续文件浏览器用） |
| RDP | 设备重定向虚拟盘 | `enable-drive=true` + `drive-path`（guacd 容器卷） | 上行=写入虚拟盘（在 RDP 会话内呈现为一块网络盘）；下行=在会话内把文件拷进该盘，guacd 即向浏览器推 `file` 流 |
| VNC | — | — | RFB 协议无文件通道，UI 明示不支持（协议层不可能，不是实现缺口） |

前端上传用 `client.createFileStream(mimetype, filename)` 起 `file` 出站流，
`Guacamole.BlobWriter` 负责 Blob→base64 分块；下载在 `client.onfile`
逐 blob 收集（每 blob ack）→ `Blob` → `URL.createObjectURL` 触发浏览器下载。

### 协议与数据流

```
上行：拖拽/选择文件 → createFileStream → file 指令（index,mimetype,name）
  → BlobWriter.sendBlob(file)（blob 指令 base64 分块）→ sendEnd
  → guacd：SFTP 写目标机 / RDP 写虚拟盘 → ack 流控（BlobWriter 自动处理）
下行（被动）：guacd 发 file 指令（mimetype,filename）→ onfile
  → onblob 收集 + sendAck(0) 流控 → onend → 浏览器下载
```

网关数据面零改动（`file`/`blob`/`ack` 全透传）；改动仅在 connect 参数表
（enable-sftp / enable-drive / drive-path）与前端。

### 与既有链路衔接

- 票据无需新字段：SSH/RDP 的文件通道随会话默认开启（不开就要按会话二次
  协商，收益为负——通道存在不代表能绕过远端文件系统权限，鉴权仍在 SSH/
  RDP 凭证层）；
- 部署：RDP 虚拟盘落在 guacd 容器的 `drive-path`（compose 挂卷，见
  `deployments/guacd/`），磁盘占用归属部署方运维职责；
- 审计限制（如实记录）：网关不解析流，**无法逐文件审计**——票据/连接/
  断开审计携带协议与会话，文件级审计需要 guacd 侧日志（GUACD_LOG_LEVEL=info
  不打印文件名，debug 又泄口令，均不合适）。接受此限制，写入本文档。

### 备选与否决

| 方案 | 否决理由 |
|---|---|
| 网关侧中转存储（浏览器→server→guacd 落盘再转） | 双份存储 + 网关要理解流语义 + 大文件占 server 内存/磁盘；透传本身就是 Guacamole 协议的设计意图 |
| SSH 终端内 zmodem/sz-rz | guacd 的 SSH 终端是服务端仿真的像素流，浏览器侧没有终端仿真层可挂 zmodem；要挂就得换 xterm.js 本地终端——等于放弃 guacd 终端，重来一遍 |
| 给 VNC 补文件通道（经 agent 中转） | 让 runtime 参与像素面数据链路，违反 §4.3「runtime 零参与」；且 agent 不在 VNC 会话里，凭空造一条旁路 |
| SSH/SFTP 文件浏览器 UI（第一版） | guacd `filesystem` 对象 + `get`/`put` 指令已支持，但目录树 UI（浏览/导航/进度）是一整块前端工作量，与拖拽传输的收益不成比例；列为 P1 后续项 |

---

## 16. 阶段二：会话录制与检索（DG-9）

### 选型与理由

录制用 guacd 原生 session recording：connect 参数注入 `recording-path`/
`recording-name`/`create-recording-path`，guacd 把会话的图形指令流按时间戳
写成 `.mjs` 文件（Guacamole session 格式，官方 `guacenc` 可离线转码 mp4）。
**零新增第三方组件**——录制器内建于 libguac，权威实现只有一份。

安全默认：`recording-include-keys=false`（**永不**把按键内容写进录像——
录像里出现明文口令是审计资产变泄漏源）；`recording-exclude-mouse=false`
（鼠标轨迹是排障关键信息，保留）。

### 协议与数据流

```
票据申请（record=true）→ 校验录制已配置（WINGMAN_GUACD_RECORDING_PATH +
WINGMAN_RECORDING_DIR 均已设，否则 400）→ WS 握手时 connect 参数注入：
  recording-path = WINGMAN_GUACD_RECORDING_PATH（guacd 容器内路径）
  recording-name = {agentID}-{sessionID}.mjs（网关生成，与审计同源唯一）
  create-recording-path = true
  recording-include-keys = false / recording-exclude-mouse = false
→ guacd 写 .mjs 到 recording-path（与 Go server 共享的卷）
→ Go server 检索 API 读共享卷（WINGMAN_RECORDING_DIR，server 侧挂载点）：
  GET    /api/remote/recordings          列表（name/size/modifiedAt）  desktop:view
  GET    /api/remote/recordings/:name/download  下载                    desktop:view
  DELETE /api/remote/recordings/:name    删除                          desktop:control
```

两个路径配置分离（guacd 视角 / server 视角）是因为两者在不同容器里，卷挂载
点可以不同；compose 示例用同一路径只是约定俗成。

### 与既有链路衔接

- 权限：复用 desktop:view / desktop:control 两权限点，**不新增 RBAC 码**
  （录像是桌面会话的衍生物，受众与操作者同源；新增权限码会让角色矩阵
  膨胀而无新语义）；
- 审计：票据审计 meta 增记 `record`；下载与删除各记一条审计
  （`desktop.recording_download` / `desktop.recording_delete`）；列表高频
  不审计（与 agents 列表同策略）；
- 文件名安全：检索 API 对 name 做 basename 校验（拒绝路径分隔符与 `..`），
  录像目录外的文件不可达；
- 回放：第一版提供下载（本地 `guacenc -t <in.mjs> <out.mp4>` 转码后观看）；
  浏览器内直接回放需官方 session-player（非 npm 分发），列为远期。

### 备选与否决

| 方案 | 否决理由 |
|---|---|
| 浏览器端 MediaRecorder 录 canvas | 丢输入时序、帧率受渲染节流影响、依赖前端全程在线；录出的视频也无法用 guacenc 工具链处理 |
| guacd 容器内 guacenc 实时转 mp4 | guacd 镜像不带 guacenc；每会话一路 ffmpeg 级转码 CPU；mp4 不可流式追加，会话中途崩溃丢整段——.mjs 恰好是崩溃安全的（逐指令追加） |
| 经 agent 截图帧序列录制 | 重回「控制面扛视频流」的老路（§1.1 已否决）；且录制从此依赖 agent 在线 |
| 录像自动清理/保留策略 | 第一版只做手动删除；保留策略是运维策略不是协议问题，等真实部署规模出现再设计 |
