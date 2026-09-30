package com.wingman.agent

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * A3 核心看门狗判定（WatchdogPolicy）JVM 单测：期望运行时的
 * running 状态解析（含缺字段/坏 JSON/空串的保守重拉）与
 * 期望停止时的恒不重拉。
 */
class WatchdogPolicyTest {

    @Test
    fun notExpectingRunningNeverRestarts() {
        assertFalse(WatchdogPolicy.shouldRestartCore(null, expectRunning = false))
        assertFalse(WatchdogPolicy.shouldRestartCore("""{"running":false}""", expectRunning = false))
        assertFalse(WatchdogPolicy.shouldRestartCore("garbage", expectRunning = false))
    }

    @Test
    fun coreRunningFalseTriggersRestart() {
        assertTrue(WatchdogPolicy.shouldRestartCore("""{"running":false}""", expectRunning = true))
    }

    @Test
    fun coreRunningTrueDoesNotRestart() {
        assertFalse(WatchdogPolicy.shouldRestartCore("""{"running":true}""", expectRunning = true))
    }

    @Test
    fun missingRunningFieldTreatedAsNotRunning() {
        // nativeStatus 契约含 running；缺字段视为不在跑（保守重拉，幂等无害）
        assertTrue(WatchdogPolicy.shouldRestartCore("""{"connectionState":"connected"}""", expectRunning = true))
    }

    @Test
    fun unparseableOrEmptyStatusTreatedAsNotRunning() {
        assertTrue(WatchdogPolicy.shouldRestartCore(null, expectRunning = true))
        assertTrue(WatchdogPolicy.shouldRestartCore("", expectRunning = true))
        assertTrue(WatchdogPolicy.shouldRestartCore("   ", expectRunning = true))
        assertTrue(WatchdogPolicy.shouldRestartCore("not json {", expectRunning = true))
    }

    @Test
    fun runningFalseAlongsideConnectedStateStillRestarts() {
        // 连接断开（RemoteClient 自愈）与核心不在跑是两回事：
        // 本用例钉「看门狗只看 running，不看 connectionState」
        val json = """{"connectionState":"disconnected","running":true}"""
        assertFalse(WatchdogPolicy.shouldRestartCore(json, expectRunning = true))
    }
}
