package com.wingman.agent

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * A3 崩溃退避策略（RestartPolicy）JVM 单测：指数退避进度、窗口内外
 * 的串归并/新开、放弃阈值、封顶与钳位防御。
 */
class RestartPolicyTest {

    private val t0 = 1_000_000L

    @Test
    fun firstCrashStartsNewBurstWithCountOne() {
        val next = RestartPolicy.nextState(null, t0)
        assertEquals(1, next.crashCount)
        assertEquals(t0, next.lastCrashAtMs)
        assertEquals(t0, next.windowStartMs)
    }

    @Test
    fun crashesWithinWindowAccumulateIntoSameBurst() {
        var state = RestartPolicy.nextState(null, t0)
        // 串内 3 次崩溃（间隔远小于窗口），windowStart 不漂移
        state = RestartPolicy.nextState(state, t0 + 1_000)
        state = RestartPolicy.nextState(state, t0 + 60_000)
        assertEquals(3, state.crashCount)
        assertEquals(t0, state.windowStartMs)
        assertEquals(t0 + 60_000, state.lastCrashAtMs)
    }

    @Test
    fun crashOutsideWindowOpensNewBurst() {
        // 边界内（恰差 1ms）：仍归并同串
        var state = RestartPolicy.nextState(null, t0)
        state = RestartPolicy.nextState(
            state, t0 + RestartPolicy.CRASH_WINDOW_MS - 1)
        assertEquals(2, state.crashCount)
        assertEquals(t0, state.windowStartMs)

        // 边界外：新串
        state = RestartPolicy.nextState(state, t0 + RestartPolicy.CRASH_WINDOW_MS)
        assertEquals(1, state.crashCount)
        assertEquals(t0 + RestartPolicy.CRASH_WINDOW_MS, state.windowStartMs)
    }

    @Test
    fun delayProgressesExponentiallyAndCapsOut() {
        assertEquals(1_000L, delayForBurstCount(1))
        assertEquals(2_000L, delayForBurstCount(2))
        assertEquals(4_000L, delayForBurstCount(3))
        assertEquals(8_000L, delayForBurstCount(4))
        assertEquals(16_000L, delayForBurstCount(5))
        assertEquals(60_000L, delayForBurstCount(7))   // 已封顶
        assertEquals(60_000L, delayForBurstCount(50))  // 深度防御：恒封顶不溢出
    }

    @Test
    fun autoRestartUntilMaxAttemptsThenGivesUp() {
        // 串内前 4 次崩溃（计数 1..4）可重启；第 5 次起放弃
        for (count in 1 until RestartPolicy.MAX_ATTEMPTS) {
            val state = RestartPolicy.CrashState(count, t0, t0)
            assertTrue("count=$count should restart", RestartPolicy.shouldAutoRestart(state))
        }
        val last = RestartPolicy.CrashState(RestartPolicy.MAX_ATTEMPTS, t0, t0)
        assertFalse(RestartPolicy.shouldAutoRestart(last))
    }

    @Test
    fun giveUpStateRecoversAfterWindowElapses() {
        // 放弃后：同串再崩仍放弃；超窗新串恢复自动重启
        val exhausted = RestartPolicy.CrashState(
            RestartPolicy.MAX_ATTEMPTS, t0, t0)
        assertFalse(RestartPolicy.shouldAutoRestart(exhausted))

        val stillInside = RestartPolicy.nextState(exhausted, t0 + 5_000)
        assertEquals(RestartPolicy.MAX_ATTEMPTS + 1, stillInside.crashCount)
        assertFalse(RestartPolicy.shouldAutoRestart(stillInside))

        val freshBurst = RestartPolicy.nextState(
            exhausted, t0 + RestartPolicy.CRASH_WINDOW_MS + 1)
        assertEquals(1, freshBurst.crashCount)
        assertTrue(RestartPolicy.shouldAutoRestart(freshBurst))
    }

    private fun delayForBurstCount(count: Int): Long =
        RestartPolicy.restartDelayMs(RestartPolicy.CrashState(count, t0, t0))
}
