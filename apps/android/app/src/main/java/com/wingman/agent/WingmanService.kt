package com.wingman.agent

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.SharedPreferences
import android.os.IBinder
import android.util.Log
import org.json.JSONObject

/**
 * 前台服务（A1，docs/android-agent-design.md §5.4）：托管 C++ 核心生命周期。
 *
 * 保活基线：前台服务优先级 + START_STICKY（崩溃后系统拉起，进程内 C++ 状态
 * 重建即重连）。完整自愈三件套（开机自启/崩溃自重启/断连自治）在 A3。
 */
class WingmanService : Service() {

    companion object {
        const val ACTION_START = "com.wingman.agent.action.START"
        const val ACTION_STOP = "com.wingman.agent.action.STOP"
        private const val CHANNEL_ID = "wingman_agent"
        private const val NOTIFICATION_ID = 1
        private const val TAG = "WingmanService"
    }

    private lateinit var prefs: SharedPreferences

    override fun onCreate() {
        super.onCreate()
        prefs = getSharedPreferences("wingman", MODE_PRIVATE)
        createChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopCore()
                stopSelf()
                return START_NOT_STICKY
            }
            else -> {
                startForeground(NOTIFICATION_ID, buildNotification())
                startCore()
            }
        }
        // START_STICKY：进程被杀后系统以 null intent 重启，走 startCore 重建链路
        return START_STICKY
    }

    override fun onDestroy() {
        stopCore()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun startCore() {
        val capabilities = JSONObject().apply {
            put("apiLevel", android.os.Build.VERSION.SDK_INT)
            put("abi", android.os.Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown")
            put("model", android.os.Build.MODEL)
        }
        val config = JSONObject().apply {
            put("serverIp", prefs.getString("serverIp", "192.168.1.10"))
            put("serverPort", prefs.getInt("serverPort", 8888))
            put("agentId", prefs.getString("agentId", "android-" + android.os.Build.MODEL))
            put("hostname", android.os.Build.MODEL)
            put("platform", "android")
            put("capabilitiesJson", capabilities.toString())
        }
        val started = WingmanJni.nativeStart(config.toString())
        Log.i(TAG, "nativeStart: $started config=$config")
    }

    private fun stopCore() {
        WingmanJni.nativeStop()
        Log.i(TAG, "nativeStop")
    }

    private fun createChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.notification_channel),
            NotificationManager.IMPORTANCE_LOW,
        )
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }

    private fun buildNotification(): Notification {
        val pi = android.app.PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            android.app.PendingIntent.FLAG_IMMUTABLE,
        )
        return Notification.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(getString(R.string.notification_text))
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setContentIntent(pi)
            .setOngoing(true)
            .build()
    }
}
