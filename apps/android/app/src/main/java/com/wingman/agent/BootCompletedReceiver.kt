package com.wingman.agent

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log

/**
 * A3 开机自启（docs/android-agent-design.md §7）：
 * BOOT_COMPLETED（开机）与 MY_PACKAGE_REPLACED（覆盖安装后）按用户开关
 * （默认关、显式开启，[BootStartGate.DEFAULT_ENABLED] 单一来源）+ 已配置
 * 服务器地址放行启动前台服务。两者均在系统的后台 FGS 启动豁免名单内
 * （API 31+ 也可 startForegroundService）。
 *
 * 显式启动（开机/更新 = 新的使用意愿）同时清零崩溃退避串。
 * Receiver 保持薄壳：放行判定在 [BootStartGate]（纯逻辑，单测直测）。
 */
class BootCompletedReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action ?: return
        if (action != Intent.ACTION_BOOT_COMPLETED &&
            action != Intent.ACTION_MY_PACKAGE_REPLACED
        ) return

        val prefs = context.getSharedPreferences(AgentPrefs.NAME, Context.MODE_PRIVATE)
        val enabled = prefs.getBoolean(AgentPrefs.KEY_AUTO_START_ON_BOOT, BootStartGate.DEFAULT_ENABLED)
        val serverIp = prefs.getString(AgentPrefs.KEY_SERVER_IP, null)
        if (!BootStartGate.shouldAutoStart(enabled, serverIp)) {
            Log.i(TAG, "boot auto-start skipped (enabled=$enabled serverIp=$serverIp)")
            return
        }

        CrashRestartHandler.clearCrashState(context)
        val start = Intent(context, WingmanService::class.java)
            .setAction(WingmanService.ACTION_START)
        try {
            context.startForegroundService(start)
            Log.i(TAG, "boot auto-start triggered")
        } catch (e: Exception) {
            // 防御：豁免名单内理论上不会拒绝；真被拒时如实记录
            Log.w(TAG, "boot auto-start rejected", e)
        }
    }

    companion object {
        private const val TAG = "BootCompleted"
    }
}
