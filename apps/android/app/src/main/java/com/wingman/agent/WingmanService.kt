package com.wingman.agent

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.ServiceCompat
import org.json.JSONObject

/**
 * 前台服务（A1，docs/android-agent-design.md §5.4；A2 投屏扩展 §5.6）：
 * 托管 C++ 核心生命周期与投屏采集。
 *
 * 保活基线：前台服务优先级 + START_STICKY（崩溃后系统拉起，进程内 C++ 状态
 * 重建即重连）。完整自愈三件套（开机自启/崩溃自重启/断连自治）在 A3。
 *
 * A2 时序硬约束（API 34）：ACTION_START_CAPTURE 分支必须先以 mediaProjection
 * 类型 startForeground，之后才能 getMediaProjection + createVirtualDisplay，
 * 否则 SecurityException。
 */
class WingmanService : Service() {

    companion object {
        const val ACTION_START = "com.wingman.agent.action.START"
        const val ACTION_STOP = "com.wingman.agent.action.STOP"
        const val ACTION_START_CAPTURE = "com.wingman.agent.action.START_CAPTURE"
        const val ACTION_STOP_CAPTURE = "com.wingman.agent.action.STOP_CAPTURE"
        const val EXTRA_RESULT_CODE = "resultCode"
        const val EXTRA_RESULT_DATA = "resultData"
        private const val CHANNEL_ID = "wingman_agent"
        private const val NOTIFICATION_ID = 1
        private const val TAG = "WingmanService"
    }

    private lateinit var prefs: SharedPreferences
    private var captureManager: ScreenCaptureManager? = null

    override fun onCreate() {
        super.onCreate()
        prefs = getSharedPreferences("wingman", MODE_PRIVATE)
        createChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopCapture()
                stopCore()
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_START_CAPTURE -> {
                startForegroundWithTypes()
                startCapture(intent)
            }
            ACTION_STOP_CAPTURE -> {
                stopCapture()
            }
            else -> {
                startForegroundWithTypes()
                startCore()
            }
        }
        // START_STICKY：进程被杀后系统以 null intent 重启，走 startCore 重建链路
        return START_STICKY
    }

    override fun onDestroy() {
        // 顺序约束：先停投屏（释放 VirtualDisplay/帧流）再停 C++ 核心
        stopCapture()
        stopCore()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun startForegroundWithTypes() {
        val notification = buildNotification()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            ServiceCompat.startForeground(
                this, NOTIFICATION_ID, notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION or
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun startCapture(intent: Intent) {
        @Suppress("DEPRECATION")
        val resultData = intent.getParcelableExtra<Intent>(EXTRA_RESULT_DATA)
        val resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, Int.MIN_VALUE)
        if (resultData == null || resultCode == Int.MIN_VALUE) {
            Log.w(TAG, "startCapture missing consent extras")
            return
        }
        if (captureManager == null) {
            captureManager = ScreenCaptureManager(this)
        }
        captureManager?.start(resultCode, resultData)
    }

    private fun stopCapture() {
        captureManager?.stop()
        captureManager = null
    }

    private fun startCore() {
        val capabilities = JSONObject().apply {
            put("apiLevel", Build.VERSION.SDK_INT)
            put("abi", Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown")
            put("model", Build.MODEL)
            // A2：注入/采集能力状态随注册上报（Dashboard 可见）
            put("hasAccessibility", isAccessibilityEnabled())
        }
        val config = JSONObject().apply {
            put("serverIp", prefs.getString("serverIp", "192.168.1.10"))
            put("serverPort", prefs.getInt("serverPort", 8888))
            put("agentId", prefs.getString("agentId", "android-" + Build.MODEL))
            put("hostname", Build.MODEL)
            put("platform", "android")
            put("capabilitiesJson", capabilities.toString())
            // 注册鉴权 token（可空；server 开启 WINGMAN_AGENT_TOKENS 时必填）
            put("authToken", prefs.getString("serverToken", "") ?: "")
            // A2：模板图根目录（wingman.vision.findImage 相对路径解析根）
            put("filesDir", getExternalFilesDir(null)?.absolutePath ?: "")
        }
        val started = WingmanJni.nativeStart(config.toString())
        Log.i(TAG, "nativeStart: $started config=$config")
    }

    private fun stopCore() {
        WingmanJni.nativeStop()
        Log.i(TAG, "nativeStop")
    }

    private fun isAccessibilityEnabled(): Boolean {
        val setting = android.provider.Settings.Secure.getString(
            contentResolver, android.provider.Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES)
        return setting?.contains(packageName) == true
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
