package com.wingman.agent

/**
 * A3 开机自启的放行判定（docs/android-agent-design.md §7）。
 *
 * 纯逻辑、无 Android 依赖，JVM 单测直测（app/src/test）。 Receiver 保持
 * 薄壳，只做参数装配与本判定调用。
 *
 * 语义：用户开关（默认开）且服务器地址非空白才放行——从未配置过的
 * 设备开机即用默认 IP 空转没有意义。
 */
object BootStartGate {

    fun shouldAutoStart(autoStartEnabled: Boolean, serverIp: String?): Boolean =
        autoStartEnabled && !serverIp.isNullOrBlank()
}
