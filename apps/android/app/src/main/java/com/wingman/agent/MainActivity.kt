package com.wingman.agent

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject

/**
 * 主界面（A1，docs/android-agent-design.md §5.4；A2 投屏授权 §5.6）：
 * 服务器配置 + 启停 + 状态轮询（1s nativeStatus）+ 权限引导
 * （无障碍设置入口、投屏授权对话框）。
 */
class MainActivity : AppCompatActivity() {

    private lateinit var prefs: android.content.SharedPreferences
    private lateinit var statusView: TextView
    private val uiHandler = Handler(Looper.getMainLooper())

    // 投屏授权（MediaProjection）：结果转发前台服务；Android 14 起授权
    // 单服务会话一次性，每次重开投屏都会再走一遍系统对话框（符合预期）
    private val projectionConsent =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == RESULT_OK && result.data != null) {
                startService(Intent(this, WingmanService::class.java).apply {
                    action = WingmanService.ACTION_START_CAPTURE
                    putExtra(WingmanService.EXTRA_RESULT_CODE, result.resultCode)
                    putExtra(WingmanService.EXTRA_RESULT_DATA, result.data)
                })
                Toast.makeText(this, R.string.toast_capture_started, Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, R.string.toast_capture_denied, Toast.LENGTH_SHORT).show()
            }
        }

    private val pollStatus = object : Runnable {
        override fun run() {
            runCatching { WingmanJni.nativeStatus() }.onSuccess { json ->
                statusView.text = formatStatus(json)
            }
            uiHandler.postDelayed(this, 1000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        prefs = getSharedPreferences("wingman", MODE_PRIVATE)

        val ipView = findViewById<EditText>(R.id.editServerIp)
        val portView = findViewById<EditText>(R.id.editServerPort)
        val agentView = findViewById<EditText>(R.id.editAgentId)
        val tokenView = findViewById<EditText>(R.id.editServerToken)

        ipView.setText(prefs.getString("serverIp", "192.168.1.10"))
        portView.setText(prefs.getInt("serverPort", 8888).toString())
        agentView.setText(prefs.getString("agentId", "android-" + android.os.Build.MODEL))
        tokenView.setText(prefs.getString("serverToken", ""))

        findViewById<Button>(R.id.btnStart).setOnClickListener {
            saveConfig(ipView, portView, agentView, tokenView)
            startForegroundService(Intent(this, WingmanService::class.java).apply {
                action = WingmanService.ACTION_START
            })
            Toast.makeText(this, R.string.toast_started, Toast.LENGTH_SHORT).show()
        }

        findViewById<Button>(R.id.btnStop).setOnClickListener {
            startService(Intent(this, WingmanService::class.java).apply {
                action = WingmanService.ACTION_STOP
            })
            Toast.makeText(this, R.string.toast_stopped, Toast.LENGTH_SHORT).show()
        }

        findViewById<Button>(R.id.btnCapture).setOnClickListener {
            val manager = getSystemService(android.content.Context.MEDIA_PROJECTION_SERVICE)
                as android.media.projection.MediaProjectionManager
            projectionConsent.launch(manager.createScreenCaptureIntent())
        }

        // 无障碍能力启用入口（注入前置）
        findViewById<Button>(R.id.btnAccessibility).setOnClickListener {
            startActivity(Intent(android.provider.Settings.ACTION_ACCESSIBILITY_SETTINGS))
        }

        statusView = findViewById(R.id.textStatus)
    }

    override fun onResume() {
        super.onResume()
        uiHandler.post(pollStatus)
    }

    override fun onPause() {
        super.onPause()
        uiHandler.removeCallbacks(pollStatus)
    }

    private fun saveConfig(ipView: EditText, portView: EditText, agentView: EditText, tokenView: EditText) {
        prefs.edit()
            .putString("serverIp", ipView.text.toString().trim())
            .putInt("serverPort", portView.text.toString().trim().toIntOrNull() ?: 8888)
            .putString("agentId", agentView.text.toString().trim())
            .putString("serverToken", tokenView.text.toString().trim())
            .apply()
    }

    private fun formatStatus(json: String): String {
        val obj = runCatching { JSONObject(json) }.getOrNull() ?: return json
        val script = obj.optJSONObject("script") ?: JSONObject()
        return getString(
            R.string.status_format,
            obj.optString("connectionState", "unknown"),
            if (obj.optBoolean("running")) "yes" else "no",
            script.optString("executionId", "-"),
            if (script.optBoolean("running")) "yes" else "no",
        )
    }
}
