> [中文](remote-gateway-guacamole-design.md) | **English**

# Remote Gateway (Guacamole) Design

**No English translation of this document yet.** The authoritative text is the Chinese document [`remote-gateway-guacamole-design.md`](remote-gateway-guacamole-design.md) (714 lines). It has not been translated yet: a rush translation of a 714-line design record would be lower quality than no translation at all, so this page carries only the language switcher and an exact outline of the source sections. The follow-up is tracked in [`development-todo.md`](development-todo.md).

## Sections of the Chinese source

- 0. 一页总结
- 1. 背景与动机：为什么需要"像素面"
  - 1.1 现有控制面能做什么、不能做什么
  - 1.2 为什么现在做
- 2. 分层定案：控制面与像素面的协议边界
  - DG-1：自动化控制面保留现有 Agent TCP（16 字节头 + JSON 体），不自行开发改道、不换行业协议栈
  - DG-2：像素面（人看 + 接管）采用 Guacamole 协议 + guacd，不自行开发像素流
- 3. 备选方案对比（为什么不是它们）
- 4. 总体架构与数据流
  - 4.1 组件与流向
  - 4.2 Go 桥在 guacd 握手中的角色
  - 4.3 runtime 的角色：只拨号、零新增监听面
  - 4.4 endpoint 地址的解析
  - 4.5 目标转发中继（DG-10，2026-10-02）
- 5. 桌面平台 endpoint 矩阵（DG-3）
- 6. Android 与 iOS 边界
  - DG-4：Android 自动化数据面不推翻——像素采集继续走 MediaProjection 链
  - 6.1 "人看 Android"：droidVNC-NG [guacd VNC 桥的取舍（单独成节]
  - DG-5：iOS 明确不覆盖
- 7. 与 cockpit 共享（DG-6）
  - 7.1 抽取落地（2026-09-25，wingman 侧先行）
- 17. 会话审计报表（P1 落地，2026-09-25）
  - 为什么另立专表而不是复用 AuditLog
  - 落库时机：会话结束时一次性写终态行
  - 录像可关联
  - 查询接口
  - 安全与权限
  - 已知取舍
- 8. 安全模型
- 9. 硬约束合规核查表
- 10. 风险与"不做"清单
- 11. 实施分期
- 12. 对既有文档的修订点
- 13. P0 实现验证备注（2026-09-23，三协议 e2e 实测）
  - 13.1 Guacamole 协议要点（实现固化的坑）
  - 13.2 guacd 版本锁定：1.5.5（1.6.0 双崩溃）
  - 13.3 对 cockpit 的反哺项（P1 抽取前核对）
  - 13.4 e2e 容器栈补强（2026-09-25）
- 14. 阶段二：剪贴板控制 UI（DG-7）
  - 选型与理由
  - 协议与数据流
  - 与既有链路衔接
  - 备选与否决
- 15. 阶段二：文件传输（DG-8）
  - 选型与理由
  - 协议与数据流
  - 与既有链路衔接
  - 备选与否决
  - 15.1 SSH/SFTP 文件浏览器（第一版：浏览/下载/上传，2026-09-26 实现）
  - 15.2 文件浏览器第二版（2026-09-26 实现）：分页/进度/重试/审计
- 16. 阶段二：会话录制与检索（DG-9）
  - 选型与理由
  - 协议与数据流
  - 与既有链路衔接
  - 备选与否决
- 18. 密钥保险箱（DG-11，P1 落地，2026-10-03）
  - 动机与定位
  - 加密结构（`internal/security/vault.go` + `models/vault.go`）
  - 端点与审计（`/api/remote/vault/*`，挂 desktop:view/control 权限组）
  - 与硬约束的相容性
