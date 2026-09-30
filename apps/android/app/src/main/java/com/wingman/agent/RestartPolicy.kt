package com.wingman.agent

/**
 * A3 崩溃自重启的退避与放弃策略（docs/android-agent-design.md §7）。
 *
 * 纯逻辑、无 Android 依赖，JVM 单测直测（app/src/test）。
 *
 * 语义：把「距本串首次崩溃不足 [CRASH_WINDOW_MS]」的连续崩溃视为同一串
 * （burst）。第 N 次崩溃的重启延迟为 BASE_DELAY_MS * 2^(N-1)，封顶
 * [MAX_DELAY_MS]；串内崩溃次数达到 [MAX_ATTEMPTS] 后放弃自动重启
 * （防崩溃风暴循环——持续崩溃说明环境已不适合无人值守重启，等待用户
 * 或下一次开机/手动启动清零）。串首间隔超窗的新崩溃开启新串（计数归 1）。
 */
object RestartPolicy {

    /** 串内最多自动重启次数（第 MAX_ATTEMPTS 次崩溃起放弃）。 */
    const val MAX_ATTEMPTS = 5

    /** 崩溃串判定窗口：距串首不足该值的崩溃归入同串。 */
    const val CRASH_WINDOW_MS = 10 * 60_000L

    /** 首次崩溃后的重启延迟基数（指数退避 1s/2s/4s/...）。 */
    const val BASE_DELAY_MS = 1_000L

    /** 重启延迟封顶（AlarmManager 精度下的实际值可能略有出入）。 */
    const val MAX_DELAY_MS = 60_000L

    /** 持久化的崩溃状态（读写方：[CrashRestartHandler]）。 */
    data class CrashState(
        val crashCount: Int,
        val lastCrashAtMs: Long,
        val windowStartMs: Long,
    )

    /** 计入一次发生在 [nowMs] 的新崩溃，返回推进后的状态。 */
    fun nextState(state: CrashState?, nowMs: Long): CrashState {
        if (state == null) return CrashState(1, nowMs, nowMs)
        val inWindow = nowMs - state.windowStartMs < CRASH_WINDOW_MS
        return if (inWindow) {
            CrashState(state.crashCount + 1, nowMs, state.windowStartMs)
        } else {
            CrashState(1, nowMs, nowMs)
        }
    }

    /** 是否仍应自动重启（放弃后等待用户/开机/手动启动清零）。 */
    fun shouldAutoRestart(state: CrashState): Boolean = state.crashCount < MAX_ATTEMPTS

    /** 本次崩溃后的重启延迟。 */
    fun restartDelayMs(state: CrashState): Long {
        val exp = (state.crashCount - 1).coerceAtLeast(0)
            .coerceAtMost(20) // 防御性钳位：Long 溢出前远早被 MAX_DELAY 封顶
        return (BASE_DELAY_MS shl exp).coerceAtMost(MAX_DELAY_MS)
    }
}
