package com.wingman.agent

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log

/**
 * A3 崩溃自重启的闹钟腿（docs/android-agent-design.md §7）：
 * [CrashRestartHandler] 在崩溃时按退避延迟调度，到点后以 ACTION_START
 * 拉起前台服务（不清零崩溃串，保持退避累积）。
 *
 * best-effort：API 31+ 后台 FGS 启动限制下 startForegroundService 可能
 * 抛 ForegroundServiceStartNotAllowedException——吞掉并记录，系统
 * START_STICKY 路径兜底。
 */
class CrashAlarmReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != ACTION_RESTART) return
        val start = Intent(context, WingmanService::class.java)
            .setAction(WingmanService.ACTION_START)
            .putExtra(WingmanService.EXTRA_FROM_CRASH_RESTART, true)
        try {
            context.startForegroundService(start)
            Log.i(TAG, "crash restart fired")
        } catch (e: Exception) {
            Log.w(TAG, "crash restart rejected by system; relying on START_STICKY", e)
        }
    }

    companion object {
        const val ACTION_RESTART = "com.wingman.agent.action.RESTART_AFTER_CRASH"
        const val REQUEST_CODE = 1001
        private const val TAG = "CrashAlarm"
    }
}
