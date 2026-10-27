> [English](../game-automation-coverage.md) | **中文**

# 游戏自动化覆盖度分析

本文档记录 Wingman 当前的游戏自动化覆盖度，并与同类产品进行比较。其目的是指导后续实现并防止范围漂移。

最近校准：2026-10-15（桌面 GUI 屏幕预览/区域选择、profile 导入/导出、脚本热重载，以及 Dashboard Teams 页面；见 Product UX Gaps 与 Orchestration/Team Gaps）。上一次校准：2026-10-08（本地 ONNX 目标检测与 team 协议——投票/inbox/广播——重新校准）。再上一次校准：2026-10-04（trigger GUI 编辑与全局热键落地）。

## 定位

Wingman 的目标是基于以下能力的非侵入式游戏自动化：

- 屏幕捕获、像素/颜色/图像识别、OCR。
- 鼠标与键盘输入模拟。
- Lua/Python 脚本。
- 从桌面 UI 通过本地 IPC 控制。
- 通过 Go server 的可选中心编排，由 runtime 主动 outbound 连接。

Wingman 不应成为注入、内存读取或反作弊绕过框架。

## 当前覆盖度

当前覆盖度在引擎/API 层较强，在产品 UX 层较弱。

| 领域 | 覆盖度 | 说明 |
| --- | --- | --- |
| 屏幕捕获 | 高 | 支持截图与位图操作。 |
| 像素/颜色检测 | 高 | 覆盖冷却时间、血条、资源指示器、UI 状态检查。 |
| 图像/模板匹配 | 中-高 | 核心能力已具备；服务端模板素材分发与脚本级模板注册表已经落地，但 GUI 采集/裁剪工具与按 profile 绑定仍缺。 |
| OCR | 中 | 对 UI 提示与文本检测有用；鲁棒性取决于预处理与 OCR 质量。 |
| 输入模拟 | 高 | 鼠标与键盘自动化覆盖 macro 与动作执行。 |
| Macro 录制/回放 | 中 | 核心录制/回放已具备，但时间线编辑与用户工作流尚未完成。 |
| Trigger 系统 | 引擎层高 | 条件/动作/冷却已与游戏自动化需求对齐。GUI 编辑（创建/编辑/删除，条件/动作/冷却字段）已在桌面 GUI trigger 页面落地。 |
| 脚本 | 高 | Lua 与 Python 支持使 Wingman 强于许多简单 macro 工具。 |
| FSM / 行为树 / 任务编排 | 高 | 适合战斗循环、采集循环与复杂状态处理。 |
| 拟人化输入 | 中-高 | 随机延迟与拟人移动方向已正确；调优 UI 与 profile 仍缺。 |
| 游戏 profile | 中 | 已有基本的 profile 方向；需要更强的按游戏素材/配置绑定。 |
| 本地 UI 控制 | 中 | IPC 架构正确；桌面 UI 已有 trigger 编辑、profile 管理与屏幕/区域工作流，但还没有 runtime 调试面板。 |
| 远程编排 | 中 | 架构正确：Go server 作为中心控制器，runtime 主动 outbound 连接。协议与 UX 仍需成熟。 |
| 团队协同 | 中 | 方向已有；投票、成员状态、任务分配与 server 仲裁需要产品化。 |
| 高级视觉 / ML | 中 | 已有基本 CV；可选的本地 ONNX 目标检测层已落地（`aiSetupLocal`，ML 构建档——见 `docs/api/vision.md`）。默认不启用，仍需按场景的模型/调优工作。鲁棒的场景理解是未来工作。 |
| 产品可用性 | 中-低 | 主要缺口：可视化配置、预览/调试、模板管理。 |

大致覆盖度：

- 引擎能力：70-80%。
- 游戏玩家产品体验：40-50%。
- 基于高级 bot/AI/内存的自动化：低，且内存/注入应保持在范围之外。

## 已覆盖的游戏场景

| 场景 | 状态 |
| --- | --- |
| 自动点击与简单挂机循环 | 已覆盖。 |
| 重复采集/制作动作 | 已通过 macro、输入与脚本循环覆盖。 |
| 技能冷却检测 | 已通过像素/颜色/图像检测覆盖。 |
| 血量/蓝量监控 | 已通过基于区域的颜色/像素检测覆盖。 |
| 按钮/图标出现后点击 | 基本覆盖；需要更好的模板工具。 |
| UI 提示文本检测 | 部分通过 OCR 覆盖。 |
| 复杂战斗循环 | 通过脚本、FSM、行为树与 trigger 支持。 |
| 多角色/团队协同 | 架构已有，但服务端协议与 UX 仍需工作。 |
| 面向普通玩家的无代码配置 | 尚不充分。 |
| 鲁棒的导航/寻路 | 尚不充分。 |
| 用 ML 模型的目标识别 | 尚不充分。 |
| 内存读取/注入 bot | 超出范围。 |

## 同类产品对比

仅陈述事实：每个工具自带什么，以及 Wingman 的能力边界在哪里。不做排名声明；这些是功能清单，而非基准测试结果。

| 产品 | 主要能力 | 与 Wingman 的差异 |
| --- | --- | --- |
| Chimpeon | 带 trigger 的游戏自动化、快速像素检测、键盘/鼠标/macro 动作、战斗循环、AFK 处理、提示检测。 | Wingman 另有 Lua/Python 脚本、FSM、行为树与中心编排；Chimpeon 自带面向游戏的 UX 模板，这是 Wingman 所缺的。 |
| AutoHotkey | 热键、脚本、macro 自动化、键盘/鼠标/窗口自动化、社区脚本库。 | Wingman 内置视觉/OCR/行为模块；AutoHotkey 拥有更大的第三方脚本库与更长的 Windows 历史。 |
| Pulover's Macro Creator | 基于 AutoHotkey 的 GUI macro 创建器、录制、循环、条件、变量、图像/像素搜索。 | Wingman 的编排在 server 侧运行，而脚本在本地 runtime agent 中执行；Pulover 的可视化 macro 编辑比 Wingman 走得更远。 |
| SikuliX | 图像驱动的自动化、在图像上等待/点击、OCR 支持、可视化脚本风格。 | 原语类似；Wingman 缺少 SikuliX 的高级图像 API 与基于区域的可视化工作流。 |
| TinyTask | 极简录制/回放、循环、播放速度、简单的可执行工作流。 | Wingman 覆盖更广的能力集；TinyTask 面向轻度使用的门槛更低。Wingman 还没有一键式简易模式。 |
| Macro Recorder | 鼠标/键盘录制、回放、图像/OCR 搜索、GUI 动作编辑。 | Wingman 能跑同样的场景；动作列表编辑、条件分支与可视化调试尚未构建。 |

参考资料：

- Chimpeon: https://chimpeon.com/
- Chimpeon help: https://chimpeon.com/support/help?id=2
- AutoHotkey docs: https://ahkscript.github.io/ko/docs/Program.htm
- Pulover's Macro Creator features: https://www.macrocreator.com/features/
- SikuliX docs: https://sikulix.github.io/docs
- SikuliX OCR docs: https://sikulix.github.io/docs/api/ocr/
- TinyTask: https://www.tinytask.net/
- Macro Recorder docs: https://www.macrorecorder.com/doc/
- Macro Recorder bitmap/OCR search: https://www.macrorecorder.com/doc/find/

## 主要缺口

### Product UX 缺口

- 命名区域库：把选好的区域保存为可复用名称。桌面 GUI
  屏幕页面已经能实时预览、绘制区域浮层、支持拖拽选择加数值/显示器选择器，并能读取指针下的像素颜色；缺的是把结果持久化为命名区域。
- 模板采集工具：从预览中裁剪图像并保存为命名模板。
- 模板素材管理器：按游戏 profile 整理按钮/图标/状态图像。
  服务端分发已存在（素材上传/列表/同步带 sha256
  校验），加上脚本级模板注册表；缺的是
  GUI 侧的整理与按 profile 绑定。
- Macro 时间线编辑器：重排动作、编辑延迟、插入循环/条件。
  macro 页面显示只读的会话摘要（捕获事件、速度、
  重复、状态）；编辑尚未构建。
- Runtime 调试面板：最后一张截图、匹配置信度、trigger 命中。
- Profile/模板导入导出：游戏 profile 已经能从
  桌面设置页导入/导出为 JSON；模板还没有导入/导出路径。
- 配置与 profile 热重载：不重启 runtime 即可更新配置/profile。脚本级热重载已存在（`script.reload(name)`
  会重新读取文件，若脚本处于活跃状态则重启它，GUI
  脚本页面通过 restart 动作暴露同样能力）。

已实现因而不再是缺口：带
逐指针像素颜色的实时屏幕预览、预览上拖拽区域选择、带
可复制坐标的区域浮层，以及显示器选择（桌面 GUI 屏幕页面）；游戏
profile JSON 导入/导出（桌面 GUI 设置页）；脚本热重载与
不重启 runtime 的 restart（`script.reload(name)` 加 GUI restart
动作）。

### Engine/API 缺口

- 类似 SikuliX 的高级可视化 API：`region.waitImage(...).click()`。
- 更好的 OCR 预处理：阈值化、缩放、区域过滤、语言/profile 预设。
- 多分辨率与 DPI 适配。
- 窗口相对坐标，而非仅有绝对屏幕坐标。
- 图像/OCR 操作的重试/超时/回退原语。
- 失败恢复：卡死检测、窗口丢失、游戏重启、重置流程。
- 目标检测：可选的 ONNX/YOLO 风格层已作为
  可选项存在（经 `aiSetupLocal` 的本地 provider、`WINGMAN_ENABLE_ML` 构建）。剩余缺口是它默认关闭发布，且需要按游戏的
  素材与场景调优。
- 模板与区域的素材版本化。

### Orchestration/Team 缺口

- 任务分配与 lease/ack 模型：server 以 lease/ack 语义把工作单元分配给团队
  成员（尚无 Task/Assign/lease 协议）。
- Dashboard 投票进度视图：Dashboard 已有 Teams 页面，
  列出团队状态、成员（含设备绑定与 leader 标记）和
  团队消息推送，但没有进行中的投票或其计票的视图。

已实现因而不再是缺口：server 仲裁的团队
投票（`team.vote_create` / `team.vote_cast`，票数持久化在 server 侧），
成员状态上报（`team.status_report`），server 控制的 runtime
inbox（`inbox.register` / `inbox.heartbeat` / `inbox.ack` / `inbox.report`
带 server ack），团队事件广播，以及 Dashboard Teams 页面（团队
状态、成员列表、按团队消息推送）。

## 建议优先级

1. 在现有预览/选择/选择器之上建立命名区域库。
2. GUI 模板采集与模板素材管理（服务端分发
   已存在）。
3. Macro 时间线编辑器。
4. 带命中可视化的 runtime 调试面板。
5. 窗口相对坐标系与 DPI/分辨率适配。
6. 高级区域/图像/OCR API。
7. 失败恢复原语。
8. 通过 Go server 仲裁的团队协同协议。

## 架构护栏

- Runtime 本地 UI 控制必须使用 IPC，不得使用 runtime 的 HTTP/WebSocket server。
- Runtime 必须主动 outbound 连接 Go orchestrator 以实现远程控制。
- Go server 仍是团队状态、投票、任务分配与 Dashboard 通信的唯一控制器。
- Runtime 可以消费 server 分配的工作，但不应成为独立的远程控制 server。
- 本地 `event` 应保持为进程内事件总线。
- 消息队列可作为 server 管理的基础设施或 server 分配的 runtime inbox 可以接受，但不应作为通过本地 event 模块暴露的任意远程发布 API。

## 产品方向

下一步不应是继续堆叠更多底层模块。当前核心对大多数非侵入式游戏自动化已经足够宽。最有价值的工作是把现有原语转化为面向游戏的产品工作流：

1. 捕获屏幕。
2. 选择一个区域。
3. 取一个颜色或保存一个模板。
4. 创建一个 trigger。
5. 挂接一个动作或 macro。
6. 预览/调试匹配结果。
7. 用热键与急停运行。
8. 把配置打包为游戏 profile。

该路径保持当前架构：本地 IPC 用于桌面 UI，outbound 连接用于 runtime，无注入或内存访问。
