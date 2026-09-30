package com.wingman.agent

import org.json.JSONObject

/**
 * A3 断连/进程内自愈看门狗的判定（docs/android-agent-design.md §7）。
 *
 * 纯逻辑、无 Android 依赖（org.json 在 JVM 单测下为真实实现），
 * 直测见 app/src/test。
 *
 * 语义：前台服务期望 C++ 核心在跑（用户/开机/崩溃恢复路径均走
 * startCore），而 nativeStatus 报告 running!=true（或状态不可解析）时，
 * 看门狗应重新拉起 nativeStart（幂等，配置来自持久化 prefs）。
 * 网络断开不在此列——C++ RemoteClient 自带指数退避重连与 outbox
 * 冲刷（agentcore_test 覆盖），看门狗只兜「服务活着但核心不在跑」。
 */
object WatchdogPolicy {

    fun shouldRestartCore(statusJson: String?, expectRunning: Boolean): Boolean {
        if (!expectRunning) return false
        if (statusJson.isNullOrBlank()) return true
        val status = runCatching { JSONObject(statusJson) }.getOrNull() ?: return true
        return !status.optBoolean("running", false)
    }
}
