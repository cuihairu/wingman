package com.wingman.agent

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * A3 开机自启放行判定（BootStartGate）JVM 单测。
 */
class BootStartGateTest {

    @Test
    fun enabledWithConfiguredIpPasses() {
        assertTrue(BootStartGate.shouldAutoStart(true, "192.168.1.10"))
    }

    @Test
    fun disabledNeverPasses() {
        assertFalse(BootStartGate.shouldAutoStart(false, "192.168.1.10"))
    }

    @Test
    fun blankIpFailsGate() {
        assertFalse(BootStartGate.shouldAutoStart(true, null))
        assertFalse(BootStartGate.shouldAutoStart(true, ""))
        assertFalse(BootStartGate.shouldAutoStart(true, "   "))
    }

    /**
     * 默认关回归钉（2026-09-30 按指令校正：原落地轮登记假设为默认开）。
     * 开机自启是特权行为，默认值只允许经显式决策改动——Receiver 与
     * MainActivity 都从本常量取默认，钉住它即钉住产品默认行为。
     */
    @Test
    fun bootAutoStartDefaultsToOff() {
        assertEquals(false, BootStartGate.DEFAULT_ENABLED)
        // 未显式开启（prefs 缺键 = 默认值）时，即便已配置地址也不放行
        assertFalse(BootStartGate.shouldAutoStart(BootStartGate.DEFAULT_ENABLED, "192.168.1.10"))
    }
}
