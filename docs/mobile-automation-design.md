# 手机端自动化方案设计（Appium / AutoJS 对照与选型）

> 日期：2026-09-22。状态：**设计稿（含实现，不含新实现代码）**。
> 本文回答一个问题：把 Appium 与 AutoJS 两条成熟路线拆开看清之后，
> Wingman 的手机端自动化应该长什么样、为什么这样长。
>
> 前置文档（本文在其结论上演进，不推翻）：
> - `docs/mobile-support-feasibility.md` —— 可行性调研：云控模式、三模式对比、生态
> - `docs/android-agent-design.md` —— A1 链路打通与 A2 能力闭环的工程设计（已实施）
> - `docs/ios-agent-design.md` —— iOS 主机控（I1，远期可选）
> - `docs/architecture-decisions.md` —— 硬架构约束（本文全部决策在该约束内）

---

## 1. 背景与定位：为什么要做这次对照

A1/A2 已交付的事实：Android 端侧 Agent（Kotlin 壳 + C++ 核心 + JNI 宿主桥）、
TCP 长链接云控、`dispatchGesture` 注入、MediaProjection 采集、找色找图、
`wingman.input/screen/vision` 脚本 API、`screenshot.capture` 远程截图。

这些决策当时是「可行性优先」拍下的。本设计的动机是把参照系补全：
Appium 与 AutoJS 各代表一类成熟解法，各自回答了「控件怎么拿、点击怎么注入、
脚本怎么写、会话怎么管」这些相同问题，答案却几乎相反。把两条路线的能力
边界、性能开销、稳定性、检测面拆开对照，可以：

1. **验证**已拍决策是否仍然成立（结论：核心四项全部成立，见 §3 各 D 决策）；
2. **补齐**两条路线已证明有价值、而 A1/A2 尚未覆盖的能力（控件树自动化、
   端侧 OCR——分别对应 AutoJS 的看家本领与增长点）；
3. **明确拒绝**看似强大但与端侧云控架构冲突的能力（uiautomator2 server、
   scrcpy 采集、WebDriver session 语义），并给出拒绝理由，避免后续重复论证。

一个关键的读法：**Appium 是「测试」的工具箱，AutoJS 是「自动化」的工具箱。**
测试的对象是可控的（自家 App、可装 instrumentation、可接受 USB 常连）；
自动化的对象是不可控的（任意三方 App、任意游戏、不能要求对方配合）。
Wingman 的目标场景（多开群控、游戏工作室、跨 App 效率脚本）属于后者，
这决定了整体走 AutoJS 的形制、用云控补其短板，而不是走 Appium 的形制。
后文所有具体决策都是这个总判断的展开。

---

## 2. 两条参考路线深度对比

### 2.1 Appium：WebDriver 协议的主机控体系

**架构**：PC 端 Appium Server（Node.js）接受 WebDriver HTTP 命令 → 按平台
驱动（Android 用 uiautomator2 driver）把命令翻译后经 `adb` 通道送达设备端
→ 设备端跑着两个 APK：`io.appium.uiautomator2.server`（HTTP RPC，经
`adb forward` 暴露给 PC）与 `...server.test`（instrumentation 壳，启动时需
系统弹窗确认 UiAutomation 连接）→ server 内部经 `UiAutomation` API 拿控件树、
经 `injectInputEvent` 注入事件。

**能力边界**：

- 控件树走 `AccessibilityNodeInfo` 全量快照 + `findByViewId/text/xpath`，
  对标准 View 体系与 WebView（可切 chromium driver）支持最好；
- 坐标注入（W3C actions）与控件语义操作（element.click）并存；
- 截屏走 minicap/`UiAutomation.takeScreenshot`，PC 侧还有 scrcpy 可做高性能取帧；
- **必须知道被测对象**：`appPackage/appActivity`、或至少设备已解锁且目标
  App 可被 `am start` 拉起。跨任意三方 App 的自由自动化不是它的设计目标。

**会话管理**：WebDriver session 是核心抽象——`POST /session` 携带
capabilities 建会话，命令都在 session 作用域内，`newCommandTimeout` 兜底
僵尸会话。session 语义天然匹配「一次测试任务」，不匹配「7×24 挂机」。

**性能开销**：每条命令 = PC→server HTTP → adb → 设备 HTTP → UiAutomation
调用，本机 USB 链路典型 10–50ms/命令；控件查找（xpath 尤甚）在设备端
遍历整棵树，复杂页面可达百毫秒级。批量群控时 PC 侧还要承担 N 台设备的
adb 复用与并发调度。**决策密集型循环（每步找图→决策→点击）在 Appium
形制下每步都要跨进程往返，延迟与带宽都被链路放大。**

**稳定性**：instrumentation 进程与被测 App 解耦，App 崩溃不影响 server；
但 PC↔设备链路（adb 断连、USB 抖动、adb server 重启）是稳定性短板，
Sauce/BrowserStack 类云平台为此做了厚重的设备管理补偿层。

**被检测面**：设备上多出两个特征明显的包名（`io.appium.settings` /
`io.appium.uiautomator2.server`）；UiAutomation 连接建立时部分 ROM 弹
「xx 正在使用无障碍/monitoring」提示；注入事件走 shell 侧通道。检测方
（反外挂/风控）对 Appium 特征的识别是成熟商品能力。

**适用场景**：被测 App 可控的 QA/E2E 测试、回归自动化、云真机租赁平台。
其 W3C 协议与多驱动生态是测试行业的公共资产。

### 2.2 AutoJS / AutoX / AutoJs6：无障碍服务的端侧自治

**架构**：一个普通 App（非 instrumentation），内部 = Rhino JS 引擎 +
AccessibilityService。系统持续把全窗口控件树推给无障碍服务；脚本经
选择器（`text()/id()/desc()/className()` 链式）在树上定位节点，经
`AccessibilityNodeInfo.performAction` 做语义操作（点击/滚动/聚焦），经
`dispatchGesture` 注入坐标手势。免 root、免 PC、免控端。

**能力边界**：

- 控件树对标准 View/Compose 支持好，WebView 可切无障碍节点也能拿到；
  **SurfaceView/游戏引擎渲染面拿不到任何控件**——游戏场景只能坐标+找图；
- 附带一整套自动化配套：悬浮窗控制台、录制定点、OCR（MLKit/Paddle 模型
  插件）、OpenCV 找图找色（AutoX 内置）、root 混合模式（可选增强）；
- 脚本语言是 JS（Rhino），生态脚本以单文件/工程形式在用户间流转。

**性能开销**：决策与执行同进程同线程，控件操作毫秒级、手势注入毫秒级；
找图在端侧直接读位图。**闭环延迟只取决于脚本逻辑本身，没有跨设备链路**——
这是它与 Appium 最本质的性能差异。

**稳定性**：短板都在「端侧常驻」：无障碍服务可能被系统/用户关掉、厂商
ROM 杀后台、Doze 限制。AutoJS 系用「前台服务 + 通知栏 + 悬浮窗 + 自愈
重启」对抗，Hamibot 类云控再加「掉线告警 + 脚本缓存重放」。App 崩溃 =
脚本中断（无 instrumentation 隔离层），但也因此没有双进程复杂度。

**被检测面**：注入走 `dispatchGesture`（事件带 accessibility 来源标记，
风控可检测「无合成输入」异常）；但 App 自身没有 Appium 那类固定特征包名，
伪装空间大；人性化模拟（贝塞尔轨迹、随机延迟）能显著压低启发式检出率。
**被检测面整体小于 Appium 路线，但不是零**——这点必须诚实写进产品边界。

**适用场景**：任意三方 App 的效率脚本、挂机、群控（叠加云控后）、
灰产/工作室（合规边界另说）、个人效率工具。**覆盖面广但单点深度
（如 WebView 内精确 DOM 操作）不如 Appium。**

### 2.3 六维对照

| 维度 | Appium（uiautomator2） | AutoJS（无障碍） | 对 Wingman 的含义 |
|------|------------------------|------------------|-------------------|
| 执行平面 | PC 主机控（决策在 PC） | **端侧自治**（决策在设备） | 云控实时闭环必须端侧（可行性文档 §3 已论证，再次验证） |
| 控件能力 | XPath/语义操作，WebView 深 | 选择器/语义操作，通用面广 | **端侧控件树是缺口，须补**（D4） |
| 注入通道 | UiAutomation.injectInputEvent（shell 侧） | dispatchGesture（无障碍侧） | 已选无障碍（D2），检测面更小、无需 instrumentation |
| 采集通道 | minicap/scrcpy/UiAutomation 截屏 | MediaProjection/无障碍截屏 | 已选 MediaProjection（D3），无需 adb 常连 |
| 会话语义 | WebDriver session | 脚本一跑一停 | 云控语义 = executionId 脚本上下文（D8），不引入 session |
| 常驻稳定性 | 链路是短板，双进程隔离好 | 服务常驻是短板，自愈成熟 | 自愈三件套已在 A3 计划，AutoJS 生态经验直接借鉴 |
| 被检测面 | 固定特征包名+shell 注入，识别成熟 | 无障碍标记可检，无固定特征，可做人化 | 已选无障碍（D2）；人化模拟是刚需（重申） |
| 设备依赖 | PC+adb 常连，N 台需 PC 编排 | 独立运行，WiFi/蜂窝即可 | 端侧 agent 天然达标；adb 只留给部署/预授权（D7） |
| 断 PC 影响 | 全停 | **无影响** | 云控断连自治（A3）与 AutoJS 单机韧性同构 |
| 生态 | 测试行业 W3C 标准件、云真机 | 脚本市场、灰产工具链 | 脚本生态对齐 Lua 用户群；W3C 语义仅借鉴命名 |

### 2.4 对照结论

1. **执行平面之争没有悬念**：Appium 形制（PC 决策）与 Wingman 云控的
   实时性要求正面冲突，可行性文档 §3 的对比表在性能维度上被 §2.1 的
   链路开销分析进一步坐实。
2. **Appium 真正值钱的是三样抽象**：能力上报（capabilities）、控件语义
   操作的动作语义、以及「驱动可替换」的分层。这三样都能在不引入
   WebDriver/session/HTTP 的前提下吸收（分别进 D7/D4/D2）。
3. **AutoJS 真正值钱的也是三样**：无障碍单通道同时解决注入+控件树、
   端侧 OCR/找图的闭环配套、以及被十年用户踩出来的常驻自愈经验。
   前两样进 D2/D4/D5，第三样进 A3。
4. **两条路线都不该照搬的部分**：Appium 的 session/HTTP/instrumentation
   与 AutoJS 的 JS 引擎/单机形态。前者与硬约束和云控模型冲突，后者
   与 Wingman 已有的 Lua 双语言决策冲突（architecture-decisions.md
   Scripting Language Strategy：移动端 Lua 优先）。

---

## 3. Wingman 选型与架构决策（D1–D9）

### D1 执行平面：维持端侧自治，拒绝 Appium 主机控形制

**决策**：Android 端继续 A1/A2 已落地的「端侧 Agent + 长链接云控」，
决策闭环（截屏→找图/找控件→注入）全部在设备本地完成；Go Server 只做
控制面（脚本管理、下发、编排、监控）。

**为什么**：§2.1/§2.3 已论证性能（每步跨链路往返不可接受）与韧性
（断 PC 全停 vs 断连自治）两面。此外还有一个部署面理由：Appium 形制
要求每 N 台设备配一台 PC 常驻 adb，工作室多开场景下 PC 机队本身就是
新的运维负担；端侧 Agent 只要设备有网（WiFi/蜂窝）即可纳管——这也是
Hamibot 商业模式成立的根本原因（可行性文档 §3 已引）。

**边界**：主机控形制并非全盘否定——它被保留给唯一合理的场景 iOS（I1，
设备作为桌面 runtime 的「外设屏幕」），那里端侧自治根本不存在合法通道
（ios-agent-design.md §1）。

### D2 注入通道：AccessibilityService 单通道，不引入第二条注入链

**决策**：所有触屏注入走 `WingmanAccessibilityService.dispatchGesture`
（A2 已实装）；不引入 UiAutomation/instrumentation 注入、不引入
minitouch/minicap 二进制、不引入 uiautomator2 server。

**为什么拒绝第二通道**：
- UiAutomation 注入（Appium 路线）要求 wingman 以 instrumentation 运行
  或持有 shell uid——前者把整个 App 变成测试 APK（签名/部署/检测面全面
  恶化），后者需要 root/adb 常驻，都违背端侧免 root 自治的前提；
- minitouch 类方案需要 adb 推送二进制并开本地端口，检测面与部署面双输，
  且作者已停止维护；
- 多通道意味着脚本语义要回答「这条点击走哪条链」，复杂度进入 API 面，
  收益却只是注入延迟差几毫秒。

**代价与对策**：dispatchGesture 的事件带无障碍来源标记，可被风控检测
（§2.2/§2.3）。对策不是换通道而是压低特征：人性化模拟模块（贝塞尔
轨迹、随机延迟、力度/时长抖动）是**刚需**，且应把「注入参数 人化范围」
作为脚本 API 的一等公民暴露（桌面 human 模块已有同类设计，移动端对齐）。
产品与文档必须明示：Wingman 不承诺绕过任何反自动化检测，只提供
「行为更像人」的工程手段（合规声明，与可行性文档 §7 一致）。

### D3 采集通道：MediaProjection 主、无障碍 takeScreenshot 兜底，拒绝 scrcpy 依赖

**决策**：维持 A2 已实装的双通道——MediaProjection + VirtualDisplay +
ImageReader（30–60fps，实时闭环主通道）；`takeScreenshot`（API 30+，
限流 ~1Hz）作低频/投屏未授权兜底。**不引入 scrcpy 采集**。

**为什么拒绝 scrcpy**：scrcpy 是优秀的**主机控**取帧方案，但它要求
设备持续连接一台 scrcpy-server 宿主（经 adb），等于把「adb 常连」重新
请回架构——这正是 D1 拒绝 Appium 形制的理由。其 60fps 低延迟解码链
（H.264 编码→socket→解码）在端侧没有意义：端侧闭环里采集与匹配在
同一台设备上，MediaProjection 直读 RGBA 缓存比「编码再解码」链路更短。
scrcpy 唯一的真实价值场景是**远程调试可视化**（人看设备画面），那属于
A4 的 Dashboard 设备视图增强（经 Go Server 转发），不进自动化数据面。

**兜底语义**：takeScreenshot 兜底不是「性能兜底」而是「权限兜底」——
投屏授权弹窗被拒/失效时，找色找图类低频脚本仍可用。两通道在
`AndroidCaptureSource` 后聚合（A2 已有），对上层 API 透明，本设计不
改变该边界，只要求 capabilities 上报当前激活通道（D7）。

### D4 控件树自动化：新增 wingman.ui.* 模块，AutoJS 语义、wingman 接口

**决策**：在既有 `wingman.input/screen/vision` 之上新增 `wingman.ui.*`
脚本模块，能力来自无障碍服务已持有的 `AccessibilityNodeInfo` 树：
节点查找（text/desc/id/className/可点击过滤，链式选择器）、语义操作
（click/longClick/scroll/focus/setText 经 `performAction`）、子树/父链
导航、节点属性读取（bounds/text/desc/selected）。Kotlin 侧扩展
`AndroidHostBridge` 查询接口（树快照 + 动作转发），C++ 侧照 A2 的
HostBridge→Lua API 模式落地。

**为什么是本设计最大的新增项**：A2 的注入是纯坐标语义，对「控件在
哪」的回答只有找图一条路——游戏场景足够，App 场景（原生/WebView）
是明显短板：找图对分辨率/主题/字体敏感，控件树定位对它们全部免疫。
AutoJS 十年生态证明控件树是 App 自动化的第一生产力；Appium 的
语义操作（element.click 而非坐标点击）同样是其稳定性来源（点的是
「那个按钮」而不是「那个像素」）。两者指向同一件事：**端侧控件树
能力是 A 线与成熟标杆之间最大的能力差**。

**为什么不引入 uiautomator2 server 来做这件事**：u2 的价值在「PC 侧
WebDriver 客户端生态」，其控件树数据源与 AutoJS 同为
AccessibilityNodeInfo——引入它等于在设备上跑一个 HTTP server（经
adb forward 暴露）来重复无障碍服务已有的能力，检测面（固定包名）、
部署面（多装两个 APK）、约束面（设备上新增监听口，尽管仅回环，仍与
「Agent 零监听」的架构卫生相悖）三输。**自己经宿主桥读树，代码量
是千行级，且完全落在既有 JNI 边界模式内**（architecture-decisions.md
Android Reverse JNI Bridge 一节的延伸，不新增翻译层）。

**语义对齐**：`wingman.ui.*` 的接口形状与桌面 `ui_automation` 模块
同名同形（桌面 UIA 树 / 移动无障碍树，同名 API 不同后端）——这正是
platform-abstraction「同一 API 不同租户」原则在脚本层的重演。

### D5 端侧 OCR：PaddleOCR（NDK）为候选主线，能力经 wingman.ocr.* 暴露

**决策**：移动端 OCR 引擎选型为 PaddleOCR Lite（arm64 NDK、离线），
接口挂 `wingman.ocr.*`（findText/recognize，桌面 ocr 模块同名同形）。
MLKit（Google Play services 动态下发）列为备选，不选云 OCR。

**为什么**：
- **游戏与国内环境双约束**：目标场景（手游/模拟器群控）大量运行在
  无 GMS 的国产 ROM/模拟器上，MLKit 依赖 Play services 可用性，直接
  出局为一等选项；云 OCR 出局理由与一切云依赖相同（延迟、离线、
  成本、隐私）。
- **PaddleOCR Lite** 离线模型 ~10MB 级、arm64 NEON 加速成熟、中文识别
  精度第一梯队，与 wingman「找图模板按需下发」（asset.sync 预留）的
  模式天然互补：模型随 APK 或首次使用时下发。
- **桌面一致性**：桌面 ocr 模块已有引擎抽象位，移动端换引擎不换 API，
  脚本跨端（与 D6 呼应）。
- 本设计只定选型与 API 形状；工程量（模型集成/量化/评测）归入
  A2.5 里程碑单独验收，**不因选型而阻塞 A3 可靠性主线**。

### D6 脚本 API 跨端契约：wingman.* 同名同形，差异显式文档化

**决策**：移动端脚本 API 继续遵循 A2 确立的「与桌面同名同形」原则，
新增模块（wingman.ui、wingman.ocr）同样先查桌面同名模块定接口形状，
再落移动后端。两端能力差异**不靠 API 改形表达**，靠三层显式机制：
capabilities 上报（D7）、API 文档的「平台差异」标注、桥缺失时的
降级语义（返回 false/nil，不抛错——A2 已确立）。

**为什么坚持同名同形**：这是 Wingman 对用户的核心承诺——一份脚本
认知资产跨平台复用（桌面学的 `wingman.findImage` 到手机上原样成立）。
AutoJS/Appium 生态各自绑死自己的 API 形状，脚本不可迁移；Wingman
以 ModuleDescriptor 单点定义双引擎绑定（architecture-decisions.md
Scripting Language Strategy），跨端复用的边际成本接近零，没有理由
放弃。反例警惕：为移动端「顺手」发明第二套命名（如 autojs 风格
`click(x,y)` 裸函数）短期讨好迁移用户，长期制造双语义维护税——
迁移友好靠文档对照表解决，不靠 API 杂交解决。

### D7 设备纳管与连接：server registry 扩展 capabilities，adb 只用于部署

**决策**：
- **运行时**：设备纳管完全走既有 TCP 长链接 + `agent.register`。在 A2
  已有的 `{platform, apiLevel, hasAccessibility, abi}` 基础上扩展
  capabilities 结构：`captureChannel`（mediaProjection/accessibility/
  none）、`hasAccessibilityService`（实时）、`screen`（分辨率/密度/
  朝向）、`ocrReady`、`uiTreeReady`（D4/D5 就绪位）。变更经 Notify
  上报（`device.capabilities` 预留槽位正式启用），server 存入
  AgentInfo，Dashboard 设备视图直接消费。
- **部署/预授权**：adb 只出现在部署与一次性授权场景——APK 安装、
  受限设置预授权（`appops set ... ACCESS_RESTRICTED_SETTINGS allow`）、
  Device Owner 批量纳管（MDM 标准路径）、无障碍服务自动化开启
  （`settings put secure enabled_accessibility_services`，仅
  Device Owner 权限下可用）。这些做成幂等脚本/文档（A3 已规划），
  **不出现在任何运行时链路里**。
- **多设备**：并发与分组复用 Go Server 既有 Registry/tags/batch 体系
  （architecture-decisions.md Agent Groups & Batch Operations——零
  server 架构改动即可群控 Android 设备，`platform` 字段已透传）。

**为什么 adb 不进运行时**：§2.3 已析——adb 常连把部署模型从「设备
自主上网」退化回「PC 机队+USB 果园」，是 Appium 形制的根本约束而非
可选项。Wingman 的云控价值恰恰建立在运行时零 adb 上。

**为什么 capabilities 要做实时上报**：无障碍服务被系统关闭、投屏
授权失效是 Android 常态（§2.2），server 侧若只有静态能力快照，
调度器会把脚本派给「名义可用、实际已瞎」的设备。实时能力位让
Dashboard 与调度（A4）能基于真状态决策——这是 Appium capabilities
概念在长链接体系下的正确对应物：**不是握手时的一次性声明，而是
持续的状态流**。

### D8 会话模型：executionId 脚本上下文，不引入 WebDriver session

**决策**：维持 A1 确立的执行模型——`run_script{path,content}` 下发、
`executionId` 关联日志与结果、`stop_script` 停止，一次一个脚本
（ScriptRunner 单线程状态机）。不引入 Appium 的长生命周期 session。

**为什么**：WebDriver session 语义为「有边界的一次测试任务」设计：
建会话→操作→退出（teardown），会话间状态清零。挂机/常驻自动化恰好
相反：脚本长时间运行、状态就是设备本身、没有「退出会话」概念。
A2 的 executionId 已经覆盖了 session 的有效子集（日志关联、结果
路由、并发停止），而 Appium session 剩余的价值（capability 约束
声明、超时回收）在 Wingman 里分别由 D7 的 capabilities 流与
ScriptRunner 的 stop/超时机制承担。**复用已有语义而非移植外来
语义，是为最小概念面。**

### D9 安全与权限边界：沿用 A3-P1 基线，补能力降级审计

**决策**：
- 传输与鉴权：沿用 agent-token-auth-design.md（token 白名单已落地，
  per-agent token/Keystore/challenge-response 为 A3-P2 演进）；
- 端侧权限：无障碍 + MediaProjection 双授权的用户引导沿用 A2 UX，
  新增能力降级事件上报（D7 capabilities 的负向变化写 server 日志，
  供 Dashboard 告警）；
- 脚本面：`wingman.ui.*` 的语义操作**只允许作用于用户已授权 wingman
  读取的窗口内容**（无障碍服务授权本身就是用户同意的全部范围），
  不提供绕过 FLAG_SECURE 截屏限制的任何机制；
- 合规声明：注入检测与人化模拟的边界声明沿用 D2 的表述，进用户手册。

**为什么强调降级审计**：手机端的安全事件形态与桌面不同——最大风险
不是越权命令（token 已挡）而是**静默降级**（服务被系统关掉后 agent
变成僵尸节点，脚本日志看起来正常实则无动作）。降级事件让「能力消失」
变成 server 可见的显式事件，这是云控模式对单机 AutoJS 的安全增强，
成本只是一条 Notify。

### 硬约束合规核查（与 architecture-decisions.md 对照）

| # | 约束 | 本设计 | 合规 |
|---|------|--------|------|
| 1 | Go Server 是中控 | 角色不变，新增 capabilities 消息消费与设备视图数据 | ✅ |
| 2 | Agent 主动 outbound | 不变；D7 的能力上报是既有 Notify 载体的新消息 | ✅ |
| 3 | 本地 UI 走本地 IPC | Android 无本地 UI；JNI 进程内直调 | ✅ |
| 4 | Runtime 禁 HTTP/WS server | 全部决策零新监听面：D4 控件树经宿主桥进程内、D5 OCR 端侧推理、**明确拒绝 u2 server 与 scrcpy-server**（各自理由见 D4/D3） | ✅ |
| 5 | Dashboard 只连 Go Server | 不变；设备视图/降级告警走 server API | ✅ |

---

## 4. 端到端链路设计

### 4.1 视觉链路（已落地，本设计不变）

```
MediaProjection(VirtualDisplay+ImageReader) ──推送──▶ C++ 最新帧缓存
                                                        │
脚本 wingman.vision.findColor/findImage ──▶ ImageAnalyzer（bitmap-first）
                                                        │
结果 ◀── 匹配（OpenCV 模板 / 逐像素容差） ◀──────────────┘
```

### 4.2 控件树链路（D4，新增）

```
系统 ──控件树推送──▶ WingmanAccessibilityService（Kotlin）
                          │ onAccessibilityEvent 增量维护窗口节点缓存
                          ▼
        AndroidHostBridge.uiQuery(selectorJson) ──JNI──▶ C++ wingman.ui.*
                          │                                │ 选择器求值/动作请求
                          ▼                                ▼
        AccessibilityNodeInfo.performAction ◀──── 动作应答（同步阻塞语义，
        （主线程 dispatch/perform）                 与 A2 手势 promise 模式一致）
```

要点：树数据**驻留 Kotlin 侧**（AccessibilityNodeInfo 不能跨进程持有，
快照序列化为精简 JSON——节点 id/text/desc/class/bounds/flags——过桥），
C++ 侧只做选择器匹配逻辑（纯数据，可桌面单测）；动作请求反向过桥由
Kotlin 在主线程执行 `performAction`。这与 A2 手势的 seq/promise 模式
同构，JNI 翻译层不新增模式。

### 4.3 OCR 链路（D5，新增）

```
帧缓存（复用 4.1）→ 预处理（灰度/二值化，ImageAnalyzer 已有）
   → PaddleOCR Lite 推理（独立线程池，避免阻塞脚本线程）
   → 文本框+置信度 → wingman.ocr.findText 返回坐标
```

模型资产经 asset.sync（A4 预留槽）按设备 abi 下发缓存，APK 内不内嵌
大模型（首装体积约束）。

---

## 5. 脚本 API 演进总表

| 模块 | 桌面 | 移动端现状 | 本设计新增/演进 |
|------|------|-----------|----------------|
| wingman.input | 鼠标键盘全套 | click/swipe/delay（dispatchGesture） | 人化参数一等公民化（时长/轨迹抖动范围） |
| wingman.screen | getPixel/capture/findColor/... | 同名子集（A2） | 不变 |
| wingman.vision | findColor/findImage/... | 同名子集（A2） | 不变 |
| wingman.ui | UIA 树语义操作 | **无** | **新增**：find/click/setText/scroll/wait（D4） |
| wingman.ocr | 引擎抽象 | **无** | **新增**：findText/recognize（D5） |
| wingman.human | 贝塞尔/随机延迟 | 部分内联在 input | 与 input 人化参数收敛 |
| 触发器/定时 | trigger.* 全套 | 端侧跑通（A2 后续） | 不变 |
| 宏录制 | hook 级录制 | 依赖注入事件源，**无** | 远期（录制依赖事件回读，无障碍侧无回读通道，如实标注为桌面专属能力） |

平台差异以 capabilities（D7）与文档标注表达；桥缺失一律降级返回
false/nil（既有约定）。

---

## 6. 分阶段里程碑与验收标准

| 阶段 | 内容 | 验收标准 |
|------|------|----------|
| **A2 收尾**（进行中） | 真机行为验证：dispatchGesture 真机手感、MediaProjection 息屏约束确认、takeScreenshot API 30+ 兜底 | 真机完成 development-todo 移动端清单 A2 剩余人工项；FakeHostBridge 测试全绿保持 |
| **A2.5 控件树 + OCR**（本设计核心交付） | D4 wingman.ui.* 全套；D5 wingman.ocr.findText；capabilities 扩展（D7） | ① 选择器单测（Kotlin 快照 JSON 固定样例→C++ 求值，桌面可跑）；② 真机验收：在设置 App 中经 wingman.ui 找到指定开关并点击，无需任何坐标；③ OCR 真机验收：对含已知文字的截帧 findText 命中坐标误差 ≤ 文字框高度；④ capabilities 变更（关无障碍）10s 内反映到 server |
| **A3 可靠性**（既有计划 + 本设计增强） | 自愈三件套、断连自治、受限设置预授权脚本、**降级审计**（D9） | 断网 5 分钟自治运行且重连后日志补发；杀进程/重启后 60s 内恢复纳管；预授权脚本在 5 台主流 ROM 实测通过；降级事件全链路可见 |
| **A4 多设备编排**（既有计划） | Team/inbox 端侧接入、Dashboard 设备视图（capabilities/降级告警展示、按 platform 分组、scrcpy 只读预览**可选**） | 20 台设备分组批量下发成功率 ≥ 既有 batch 基线；设备视图能区分「在线但能力降级」与「健康」设备 |
| **I1 iOS**（远期，不变） | 见 ios-agent-design.md | 启动条件不变 |

---

## 7. 风险与「不做」清单

| 项 | 决策 | 理由 |
|----|------|------|
| uiautomator2 / Appium instrumentation | **不做** | D4/D2：检测面、部署面、架构卫生三输，能力经宿主桥自建 |
| scrcpy 进自动化数据面 | **不做** | D3：adb 常连违背 D1；Dashboard 预览场景 A4 另议（只读、人工触发） |
| WebDriver/W3C 协议面 | **不做** | D8：session 语义与挂机模型冲突；能力等价物已各有归属 |
| JS 引擎兼容 AutoJS 脚本 | **不做** | 双语言决策已闭（Lua/Python）；生态迁移靠 API 对照文档 |
| 绕过 FLAG_SECURE / 反检测承诺 | **不做** | 合规红线；人化模拟是工程手段不是对抗承诺 |
| iOS 端侧 | **不做** | 无合法通道（既有结论不变） |
| 无障碍服务被杀的「强保活」（对抗系统） | **不承诺** | 自愈重启 + 降级告警是正道；与系统对抗是猫鼠游戏，进风险清单不进设计 |

新增风险：控件树快照的窗口增量大事件在高频动画界面可能堆积（对策：
快照节流 + 仅按需全量拉取，D4 实施时以性能用例验收）；PaddleOCR
Lite 在低端机的推理延迟（对策：独立线程池 + 脚本层异步语义 + A2.5
验收设低端机基线）。

---

## 8. 对既有文档的修订点（实施时同步）

1. `docs/android-agent-design.md` §5.5/§5.6：补一行「D4/D5 宿主桥扩展
   见 mobile-automation-design.md」；
2. `docs/mobile-support-feasibility.md` §8：A2 拆出 A2.5，条目指向本设计；
3. `docs/architecture-decisions.md`：D4 落地时在 Android Reverse JNI
   Bridge 一节追加 uiQuery/performAction 宿主桥扩展（实施时）；
4. `docs/development-todo.md`：A2.5 里程碑登记（实施时）。

---

## 9. 一页总结

- **形制**：AutoJS 的形制（端侧自治 + 无障碍单通道）+ 云控的调度
  （Go Server），拒绝 Appium 的形制（主机控/adb 常连/session）——
  因为目标场景是不可控的任意 App 与 7×24 挂机，且实时闭环只能在端侧。
- **吸收**：Appium 的 capabilities（升级为实时能力流 D7）、语义操作
  思想（D4 控件树）；AutoJS 的控件树生产力（D4）、端侧 OCR 配套（D5）、
  常驻自愈经验（A3）。
- **拒绝**：uiautomator2 server、scrcpy 数据面、WebDriver session、
  第二条注入链——每项拒绝都有明确的「能力等价物已就地解决」论证。
- **纪律**：一切新增零监听面、零新控制平面；脚本 API 与桌面同名同形；
  检测面诚实披露，人化模拟是工程手段而非对抗承诺。
