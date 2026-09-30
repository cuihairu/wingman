package com.wingman.agent

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.util.Log

/**
 * A3 崩溃自重启（docs/android-agent-design.md §7）：进程级未捕获异常
 * 处理器，在交给系统默认处理器（崩溃记录/对话框/进程终止）之前：
 *
 * 1. 记录崩溃状态与摘要到 prefs（退避策略输入 + 现场诊断）；
 * 2. 若崩溃前前台服务在跑且退避策略未放弃，经 AlarmManager 在退避
 *    延迟后调度一次服务重启（[CrashAlarmReceiver]）。
 *
 * 三层兜底关系：START_STICKY 是系统保证的拉起路径（不受本处理器成败
 * 影响）；本类的 AlarmManager 调度是「更快、带退避上限」的加速路径，
 * 仅 best-effort——设备处于后台 FGS 启动限制（API 31+）时
 * startForegroundService 可能被系统拒绝，此时让位于 START_STICKY。
 *
 * 只在「服务确实在跑」时重启：MainActivity 崩溃而 agent 从未启动的
 * 场景不应该把 agent 拉起来（状态恢复语义）。
 */
object CrashRestartHandler {

    private const val TAG = "CrashRestart"

    /** 由 WingmanApplication.onCreate 安装（进程级、含主线程）。 */
    fun install(context: Context) {
        val appContext = context.applicationContext
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            try {
                handleCrash(appContext, throwable)
            } catch (t: Throwable) {
                // 崩溃处理器自身绝不能再抛——否则吞掉原始崩溃
                Log.w(TAG, "crash handler failed", t)
            }
            previous?.uncaughtException(thread, throwable)
        }
    }

    /**
     * 用户/开机路径的显式启动清零崩溃串（新一轮使用意愿 = 重置退避）。
     * 崩溃恢复路径（CrashAlarmReceiver）不清零，保持退避累积。
     */
    fun clearCrashState(context: Context) {
        context.getSharedPreferences(AgentPrefs.NAME, Context.MODE_PRIVATE).edit()
            .remove(AgentPrefs.KEY_CRASH_COUNT)
            .remove(AgentPrefs.KEY_CRASH_LAST_AT)
            .remove(AgentPrefs.KEY_CRASH_WINDOW_START)
            .apply()
    }

    private fun handleCrash(context: Context, throwable: Throwable) {
        val prefs = context.getSharedPreferences(AgentPrefs.NAME, Context.MODE_PRIVATE)
        val now = System.currentTimeMillis()
        val prev = readState(prefs)
        val next = RestartPolicy.nextState(prev, now)
        // 先读门控再落盘（写入会作废服务运行标志）
        val serviceWasRunning = prefs.getBoolean(AgentPrefs.KEY_CORE_RUNNING, false)

        prefs.edit()
            .putInt(AgentPrefs.KEY_CRASH_COUNT, next.crashCount)
            .putLong(AgentPrefs.KEY_CRASH_LAST_AT, now)
            .putLong(AgentPrefs.KEY_CRASH_WINDOW_START, next.windowStartMs)
            .putString(AgentPrefs.KEY_LAST_CRASH_MESSAGE, "${throwable::class.java.simpleName}: ${throwable.message}")
            .putBoolean(AgentPrefs.KEY_CORE_RUNNING, false) // 进程已死，服务状态作废
            .apply()

        if (!serviceWasRunning) {
            Log.w(TAG, "crash while agent service not running; no auto restart")
            return
        }
        if (!RestartPolicy.shouldAutoRestart(next)) {
            Log.w(TAG, "giving up auto restart after ${next.crashCount} crashes in burst")
            return
        }
        scheduleRestart(context, RestartPolicy.restartDelayMs(next))
    }

    private fun readState(prefs: android.content.SharedPreferences): RestartPolicy.CrashState? {
        val count = prefs.getInt(AgentPrefs.KEY_CRASH_COUNT, 0)
        if (count <= 0) return null
        return RestartPolicy.CrashState(
            crashCount = count,
            lastCrashAtMs = prefs.getLong(AgentPrefs.KEY_CRASH_LAST_AT, 0L),
            windowStartMs = prefs.getLong(AgentPrefs.KEY_CRASH_WINDOW_START, 0L),
        )
    }

    private fun scheduleRestart(context: Context, delayMs: Long) {
        val intent = Intent(context, CrashAlarmReceiver::class.java)
            .setAction(CrashAlarmReceiver.ACTION_RESTART)
        // 目标是 BroadcastReceiver，必须 getBroadcast——getForegroundService
        // 会让系统到点按 service 组件解析 CrashAlarmReceiver 而恒
        // 「Unable to start service ... not found」，闹钟腿自 A3 落地起从未
        // 真正触发（API 34 模拟器 am crash 实测；此前被 START_STICKY 兜底
        // 掩盖）
        val pending = PendingIntent.getBroadcast(
            context, CrashAlarmReceiver.REQUEST_CODE, intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val alarmManager = context.getSystemService(AlarmManager::class.java) ?: return
        alarmManager.setAndAllowWhileIdle(
            AlarmManager.RTC_WAKEUP, System.currentTimeMillis() + delayMs, pending)
        Log.w(TAG, "scheduled agent restart in ${delayMs}ms")
    }
}
