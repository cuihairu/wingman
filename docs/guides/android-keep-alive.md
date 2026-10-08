# Android Agent 机型保活指引（A3）

> 适用：Wingman Android Agent（apps/android，A3 可靠性落地 2026-09-30）。
> 配套设计：`docs/android-agent-design.md` §7（生命周期与保活）。
> 姊妹篇：`docs/guides/android-restricted-settings.md`——Android 13+ 侧载
> 安装时无障碍**打不开**是受限设置问题，先看那一页再回来。

## 背景：Agent 自身的保活已经做了什么

App 侧三层机制（无需用户配置）：

| 机制 | 覆盖场景 |
|------|----------|
| 前台服务 + START_STICKY | 用户杀 App / 进程被系统回收后由系统拉起 |
| 崩溃自重启（CrashRestartHandler + 闹钟） | 进程未捕获异常后按指数退避（1s→60s 封顶）重启；10 分钟内连崩 5 次放弃，等待用户或下次开机清零 |
| 核心看门狗（WingmanService 内） | 服务存活但 C++ 核心不在跑时，每 30s 重拉 nativeStart |
| 开机自启（BootCompletedReceiver） | 开机 / App 覆盖安装后按开关（默认关，需在 App 内显式勾选）自启 |

网络断开不在此列：C++ RemoteClient 自带指数退避重连 + 有界 outbox
（断连期间事件缓存、重连后冲刷），脚本在断连期间继续本地执行。

## 为什么还需要手工设置

多数国产厂商 ROM 在 AOSP 之上叠加了激进的省电策略：即便 App 持有前台
服务，静止后台一段时间后仍可能被整进程冻结（cached/freezer）或杀死，且
**跳过 START_STICKY 的重建**（部分实现直接移除任务栈）。这不是代码能
自愈的范围——系统级的「不受信任」判定只能由用户在设置里显式豁免。

被冻结的典型症状：Dashboard 上 agent 离线，但设备端 App 图标还在、点开
后 agent 立刻恢复在线（进程被冻而非被杀）。

## 分机型设置清单

### 小米 / Redmi（MIUI / HyperOS）

1. 设置 → 应用设置 → 应用管理 → Wingman Agent → **自启动** 打开；
2. 同页 → **省电策略 → 无限制**；
3. 最近任务卡片长按/下拉 → **锁定**（加锁卡片不会被一键清理）。

### 华为 / 荣耀（EMUI / HarmonyOS / MagicOS）

1. 设置 → 应用 → 应用启动管理 → Wingman Agent → 关闭「自动管理」，
   手动管理三项全开：**允许自启动、允许关联启动、允许后台活动**；
2. 设置 → 电池 → 更多电池设置 → 关闭「休眠时始终保持网络连接」可按需
   保留（息屏断网会直接断开 agent 长链接，重连由 RemoteClient 退避接管）。

### OPPO / 一加 / realme（ColorOS / OxygenOS）

1. 设置 → 电池 → 更多 → 应用耗电管理 → Wingman Agent → **允许完全
   后台行为**；
2. 设置 → 应用管理 → Wingman Agent → 允许**自启动**；
3. 最近任务卡片锁定。

### vivo / iQOO（OriginOS / Funtouch OS）

1. 设置 → 电池 → 后台耗电管理 → Wingman Agent → **允许后台高耗电**；
2. 设置 → 快捷与辅助 / 更多设置 → vivo 手机管家 → 权限管理 → 自启动
   允许 Wingman Agent。

### 三星（One UI）

1. 设置 → 电池 → 后台使用限制：确认 Wingman Agent **不在**「从未使用的
   应用」清理名单，不加入**深度休眠应用**；
2. 需要息屏保活的设备：设置 → 电池 → 不受限制使用（若提供该选项）。

### 通用（所有机型建议）

- 系统设置 → 应用 → Wingman Agent → 电池 → **不优化**（关闭电池优化）；
- 通知权限保持开启（前台服务通知被关的 ROM 上，部分厂商会顺带降低进程优先级）；
- Android 12+ 的「通知」与「电池」设置页厂商定制差异较大，找不到对应
  入口时以「省电/后台/自启动」关键词检索系统设置。

## 验证方法

设置完成后：

1. 启动 agent，回到桌面静置 10–30 分钟（覆盖厂商常用冻结窗口）；
2. Dashboard 观察 agent 是否持续在线、心跳不中断；
3. `adb shell dumpsys activity services com.wingman.agent`（或
   `dumpsys activity services .WingmanService`）确认服务未被冻结；
   `dumpsys deviceidle` 可另看 Doze/省电白名单状态；
4. 重启设备一次，确认开机自启生效（App 未打开的情况下 agent 自动上线）。

## 已知边界（登记，非缺陷）

- **MediaProjection 授权不可恢复**：Android 14 起投屏授权单会话一次性，
  进程崩溃/重启后投屏不会自动恢复，需要重新点「开启投屏」走系统对话框
  （系统约束，A2 已登记）；agent 链路（脚本/手势/日志）不受影响。
- **息屏采集**：A2 已知前提——投屏仅在设备亮屏时出帧；息屏保活采集属
  A4 之后的事项。
- **崩溃串放弃后的恢复**：10 分钟内连崩 5 次（默认阈值）后自动重启停摆，
  需用户打开 App 手动启动或等下次开机——防止「崩溃-重启」风暴循环。
