> [中文](zh/platform-abstraction-design.md) | **English**

# Platform Abstraction Design

**No English translation of this document yet.** The authoritative text is the Chinese document [`platform-abstraction-design.md`](zh/platform-abstraction-design.md) (523 lines). It has not been translated yet: a rush translation of a 523-line design record would be lower quality than no translation at all, so this page carries only the language switcher and an exact outline of the source sections. The follow-up is tracked in [`development-todo.md`](review/development-todo.md).

## Sections of the Chinese source

- 1. 概述
  - 1.1 设计目标
  - 1.2 当前问题
  - 1.3 目标架构
- 2. 接口设计
  - 2.1 捕获接口 (ICapture)
  - 2.2 输入接口 (IInput)
  - 2.3 窗口接口 (IWindow)
  - 2.4 屏幕接口 (IScreen)
- 3. 平台工厂
  - 3.1 工厂接口
  - 3.2 使用示例
- 4. 实现策略
  - 4.1 编译时选择（推荐）
  - 4.2 运行时切换（可选）
  - 4.3 技术栈切换示例
- 5. 目录结构
- 6. 迁移路径
  - 6.1 第一阶段：创建接口层
  - 6.2 第二阶段：实现 Windows 平台
  - 6.3 第三阶段：实现 macOS/Linux
  - 6.4 状态与后续阶段（2026-10-11）
- 7. 优势总结
- 8. 薄层纪律与平台边界守卫
  - 8.1 四条纪律
  - 8.2 namespace 规范
  - 8.3 边界守卫
  - 8.4 独立库薄层
