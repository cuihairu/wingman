#pragma once

namespace wingman::runtime {

/// 订阅 EventHub 的 notify.tray.* 意图事件（脚本层 notify.trayShow/trayHide/
/// traySetBadge/traySetTooltip 经 NotifyManager 发出），转投 EventBuffer
/// （method = "tray.show"/"tray.hide"/"tray.badge"/"tray.tooltip"）。
/// GUI 经 RPC `events.drain` 拉取后驱动系统托盘（tauri TrayIcon）。
///
/// 托盘本体归 GUI 所有：runtime 只产生意图事件，无 GUI 附着时事件仅入
/// 有界缓冲、无消费者（见 lib/wingman notify 模块 NotifyManager::tray 注释）。
///
/// 幂等：重复调用不重复订阅（按固定订阅名 "notify.tray_bridge" 判定）。
/// 进程生命周期内常驻，无对应 uninstall（EventBuffer/EventHub 均为
/// 进程级单例，与 runtime 同生命周期）。
void installNotifyBridge();

/// 订阅 EventHub 的统一系统事件源（systemwatch.* / filewatcher.* /
/// trigger.fired / trigger.action / macro.state / macro.recorded），原样
/// 转投 EventBuffer（method 与事件名一致），GUI 经 `events.drain` 拉取。
/// 与托盘桥接的差异：不改名（notify.* 桥接会剥前缀）。
///
/// 幂等：重复调用不重复订阅（订阅名 "system_event_bridge"）。常驻无 uninstall。
void installSystemEventBridge();

} // namespace wingman::runtime
