# Android Agent 受限设置指引（Android 13+，A3）

> 适用：Wingman Android Agent（apps/android，A3 部署体验落地 2026-09-30）。
> 配套设计：`docs/mobile-support-feasibility.md` §5.2（受限设置三档解法）、
> `docs/mobile-automation-design.md` D7（adb 只出现在部署期）。
> 姊妹篇：`docs/guides/android-keep-alive.md`——本页解决「无障碍**开不了**」，
> 保活页解决「开了之后**被杀**」，两页互补。

## 背景：什么是「受限设置」

Android 13（API 33）起，**侧载安装**（非应用商店渠道：adb install、
直接下载 APK 安装等）的 App 默认被系统标记为「受限设置」——其受管控的
敏感权限与服务（无障碍服务是其中之一）会被**静默屏蔽**：用户进系统设置
能找到开关，但**打不开**（点了无反应，或提示受 Android 限制）。

这是端侧 Agent 开箱失败率最高的单一来源
（mobile-support-feasibility.md 风险表评级：**高**）：Agent 的一切注入能力
（A2 dispatchGesture 手势）都以无障碍服务启用为前提，受限设置不解开，
后面的步骤全部无从谈起。

**不受影响的设备**：Android 12（API 32）及以下；应用商店渠道安装的
App（多数商店安装不触发该标记）。

## 症状识别

| 症状 | 指向 |
|------|------|
| 系统设置 → 无障碍 → Wingman 注入服务，开关打不开 | 本页（受限设置） |
| 开关能打开但服务反复被停 | 保活页（厂商省电策略） |
| 开关能打开、Agent 内状态正常，注入无效 | A2 能力问题（另查） |

## 三档解法（按场景选一）

### 方案一：手动允许（单台，无需电脑）

1. 系统设置 → **应用** → **Wingman Agent** → 进入应用信息页；
2. 点右上角 **⋮（更多）** → **允许受限制的设置**（系统会弹确认）；
3. 回到 无障碍 设置，此时开关已可正常打开；
4. 回 App 点「无障碍设置」按钮核对服务已启用。

> 部分定制 ROM 的 ⋮ 菜单位置不同或入口名称略异（「允许受限制的设置」
> 也写作「允许受限设置」）；找不到入口时先升级系统应用组件或改走方案二。

### 方案二：adb 预授权（可脚本化，幂等）

仓库提供一键脚本 `scripts/android-restricted-settings.sh`（三态约定与
其他 verify-*.sh 一致：PASS=0 / FAIL=1 / SKIP=2）：

```sh
# 检查当前状态（默认动作）
scripts/android-restricted-settings.sh check
# 幂等预授权（allow 后自动复核）
scripts/android-restricted-settings.sh allow
# 只打印状态 / 还原为 default
scripts/android-restricted-settings.sh status
scripts/android-restricted-settings.sh revoke
# 多包名部署
WINGMAN_ANDROID_PKG=com.example.wingman2 scripts/android-restricted-settings.sh allow
```

判定口径：**无 adb / 无已授权设备判 SKIP**（环境未就绪，不算失败）；
包未装判 FAIL（先装 APK）；API < 33 判 PASS（不受约束）。脚本有 Go 契约
测试护栏（`orchestrator/server/integration/android_restricted_settings_script_test.go`，
假 adb 驱动、不碰真设备）。

等价的原始命令（脚本即其幂等封装）：

```sh
adb shell appops set com.wingman.agent ACCESS_RESTRICTED_SETTINGS allow
adb shell appops get com.wingman.agent ACCESS_RESTRICTED_SETTINGS
```

> **边界（设计决策 D7）**：adb 只出现在部署 / 一次性授权路径。本脚本与
> 本文档即是全部——任何运行时链路（Agent 自身、远程控制面）都不依赖 adb。

### 方案三：Device Owner / MDM 批量纳管

多设备规模化部署时，把「允许受限设置」并入移动设备管理策略，由 MDM
平台（或 `dpm set-device-owner` 注册的设备所有者 App）统一下发，免逐台
手动操作。要点：

- 前提：设备已完成 Device Owner 纳管（消费级零售机通常不适用）；
- 本仓库不附带 MDM 配置——走企业既有 MDM 平台下发同款 appops /
  受限设置策略；具体策略项名称以平台文档为准。

## App 内引导

主界面提供两层引导（A3 落地）：

- **常驻按钮**「受限设置指引（Android 13+）」：随时查看三档解法全文；
- **自动提示**：Android 13+ 且无障碍未启用时，回到 App 前台自动弹一次
  引导对话框；确认过一次后不再自动弹（按钮仍常驻）。

> 自动提示的判定口径（诚实边界）：公开 API **无法区分**「无障碍被受限
> 设置挡住」与「用户还没去开启」，系统不暴露该状态。故按
> `RestrictedSettingsPolicy`（API ≥ 33 且未启用且未确认）触发，
> Android 12 及以下不弹；「无障碍失效检测上报」依赖 device.capabilities
> 预留槽位，另行登记未做。

## 验证方法

1. （脚本路径）`check` 输出 `PASS ... allow`；
2. （手动路径）无障碍设置里开关可以正常打开并保持启用；
3. App 状态区「核心运行」之外，远程下发一条依赖注入的指令（如
   `screenshot.capture`）验证链路。

## 已知边界（登记，非缺陷）

- **ROM 差异**：个别 ROM 不支持该 appop——脚本 `allow` 后复核仍非 allow
  会明确 FAIL 并指回手动路径，不会假报成功；
- **真机验收范围**：本页步骤按各厂商官方文档口径编写并在 AOSP/类原生
  环境验证脚本行为；五厂商（小米/华为/OPPO/vivo/三星）定制 ROM 的
  ⋮ 菜单逐机型截图级验收未在本机执行（无真机环境），登记待真机抽样；
- **商店安装不触发**：经应用商店分发的构建不受受限设置约束（上架分发
  时本页仅需归档备查）。
