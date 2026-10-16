> [中文](android-agent-design.md) | **English**

# Android Agent Design

**No English translation of this document yet.** The authoritative text is the Chinese document [`android-agent-design.md`](android-agent-design.md) (559 lines). It has not been translated yet: a rush translation of a 559-line design record would be lower quality than no translation at all, so this page carries only the language switcher and an exact outline of the source sections. The follow-up is tracked in [`development-todo.md`](development-todo.md).

## Sections of the Chinese source

- 1. 目标与范围
  - 1.1 A1（本文实施范围）：链路打通
  - 1.2 非目标（后续里程碑）
  - 1.3 硬约束核查（与 `docs/architecture-decisions.md` 对齐）
- 2. 总体架构
- 3. 协议设计（复用现有帧协议，零新消息类型）
  - 3.1 复用的既有消息（Android Agent 必须实现）
  - 3.2 run_script 的 content 扩展（本设计唯一的协议变更）
  - 3.3 注册与能力上报（弱约定）
  - 3.4 预留（不在 A1 实现，仅命名占位）
- 4. Go Server 改动清单（本设计全部 server 侧改动）
- 5. Android 端设计
  - 5.1 工程结构
  - 5.2 C++ 核心层
  - 5.3 JNI 边界（窄接口）
  - 5.4 Kotlin 壳
  - 5.5 platform/android 租户（A2 已实装）
  - 5.6 反向 JNI 桥与 A2 脚本能力（A2 实装）
  - 5.7 触发器引擎（声明式定时/像素触发，A2 扩展实装）
- 6. 构建体系
  - 6.1 依赖（全部 vcpkg，遵守项目依赖管理规则）
  - 6.2 构建链路
  - 6.3 环境要求（README 展开）
- 7. 生命周期与保活（A1 最小 + A3 完整约束）
- 8. 安全（A1 基线 + A3 演进）
- 9. 测试与验证
- 10. A2 实施摘要（2026-09-21）
- 10'. A3 可靠性实施摘要（2026-09-30）
- 10''. A3 受限设置引导实施摘要（2026-09-30）
- 10'. A1 实施清单（本文对应提交的内容）
