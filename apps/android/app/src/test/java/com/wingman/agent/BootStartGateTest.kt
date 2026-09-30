package com.wingman.agent

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
}
