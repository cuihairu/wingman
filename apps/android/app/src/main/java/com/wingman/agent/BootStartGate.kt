package com.wingman.agent

/**
 * A3 开机自启的放行判定（docs/android-agent-design.md §7）。
 *
 * 纯逻辑、无 Android 依赖，JVM 单测直测（app/src/test）。 Receiver 保持
 * 薄壳，只做参数装配与本判定调用。
 *
 * 语义：用户开关（**默认关、显式开启**——2026-09-30 校正，原「默认开」为
 * 落地轮登记的假设；开机自启是特权行为，须用户显式勾选）且服务器地址
 * 非空白才放行——从未配置过的设备开机即用默认 IP 空转没有意义。
 *
 * [DEFAULT_ENABLED] 是该默认值的单一来源：Receiver 与 MainActivity 的
 * prefs 读取都必须经它，禁止再写字面量 true/false。
 */
object BootStartGate {

    /** 开机自启默认值：关（显式开启）。改动它 = 改产品默认行为，须过评审。 */
    const val DEFAULT_ENABLED = false

    fun shouldAutoStart(autoStartEnabled: Boolean, serverIp: String?): Boolean =
        autoStartEnabled && !serverIp.isNullOrBlank()
}
