package com.wingman.agent

/**
 * 受限设置引导判定（A3 部署体验，docs/mobile-support-feasibility.md §5.2）。
 *
 * 诚实边界：公开 API 无法区分「无障碍被受限设置挡住」与「用户还没开启
 * 无障碍」——系统不暴露该状态。故口径定为 API ≥ 33（受限设置生效门槛）且
 * 无障碍未启用且未确认过引导时提示；确认一次后不再自动弹（按钮入口常驻）。
 * 「无障碍失效检测上报」依赖 device.capabilities 预留槽位，另行登记未做。
 */
object RestrictedSettingsPolicy {
    /** Android 13（API 33）起，侧载 App 的受限设置开始生效。 */
    const val RESTRICTED_SETTINGS_API = 33

    fun isAffected(apiLevel: Int): Boolean = apiLevel >= RESTRICTED_SETTINGS_API

    fun shouldPrompt(
        apiLevel: Int,
        accessibilityEnabled: Boolean,
        promptAcknowledged: Boolean,
    ): Boolean = isAffected(apiLevel) && !accessibilityEnabled && !promptAcknowledged
}
