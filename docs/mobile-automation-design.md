> [中文](mobile-automation-design.md) | **English**

# Mobile Automation Design

**No English translation of this document yet.** The authoritative text is the Chinese document [`mobile-automation-design.md`](mobile-automation-design.md) (476 lines). It has not been translated yet: a rush translation of a 476-line design record would be lower quality than no translation at all, so this page carries only the language switcher and an exact outline of the source sections. The follow-up is tracked in [`development-todo.md`](development-todo.md).

## Sections of the Chinese source

- 1. 背景与定位：为什么要做这次对照
- 2. 两条参考路线深度对比
  - 2.1 Appium：WebDriver 协议的主机控体系
  - 2.2 AutoJS / AutoX / AutoJs6：无障碍服务的端侧自治
  - 2.3 六维对照
  - 2.4 对照结论
- 3. Wingman 选型与架构决策（D1–D9）
  - D1 执行平面：维持端侧自治，拒绝 Appium 主机控形制
  - D2 注入通道：AccessibilityService 单通道，不引入第二条注入链
  - D3 采集通道：MediaProjection 主、无障碍 takeScreenshot 兜底，拒绝 scrcpy 依赖
  - D4 控件树自动化：新增 wingman.ui.* 模块，AutoJS 语义、wingman 接口
  - D5 端侧 OCR：PaddleOCR（NDK）为候选主线，能力经 wingman.ocr.* 暴露
  - D6 脚本 API 跨端契约：wingman.* 同名同形，差异显式文档化
  - D7 设备纳管与连接：server registry 扩展 capabilities，adb 只用于部署
  - D8 会话模型：executionId 脚本上下文，不引入 WebDriver session
  - D9 安全与权限边界：沿用 A3-P1 基线，补能力降级审计
  - 硬约束合规核查（与 architecture-decisions.md 对照）
- 4. 端到端链路设计
  - 4.1 视觉链路（已落地，本设计不变）
  - 4.2 控件树链路（D4，新增）
  - 4.3 OCR 链路（D5，新增）
- 5. 脚本 API 演进总表
- 6. 分阶段里程碑与验收标准
- 7. 风险与「不做」清单
- 8. 对既有文档的修订点（实施时同步）
- 9. 一页总结
