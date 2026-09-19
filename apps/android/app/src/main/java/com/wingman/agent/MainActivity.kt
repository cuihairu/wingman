package com.wingman.agent

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject

/**
 * 主界面（A1，docs/android-agent-design.md §5.4）：
 * 服务器配置 + 启停 + 状态轮询（1s nativeStatus）。
 * 状态事件回调（C++ → Kotlin onCoreStatus）A2 引入，A1 轮询够用。
 */
class MainActivity : AppCompatActivity() {

    private lateinit var prefs: android.content.SharedPreferences
    private lateinit var statusView: TextView
    private val uiHandler = Handler(Looper.getMainLooper())

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

        ipView.setText(prefs.getString("serverIp", "192.168.1.10"))
        portView.setText(prefs.getInt("serverPort", 8888).toString())
        agentView.setText(prefs.getString("agentId", "android-" + android.os.Build.MODEL))

        findViewById<Button>(R.id.btnStart).setOnClickListener {
            saveConfig(ipView, portView, agentView)
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

        // A2：无障碍能力启用入口（现在是占位）
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

    private fun saveConfig(ipView: EditText, portView: EditText, agentView: EditText) {
        prefs.edit()
            .putString("serverIp", ipView.text.toString().trim())
            .putInt("serverPort", portView.text.toString().trim().toIntOrNull() ?: 8888)
            .putString("agentId", agentView.text.toString().trim())
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
