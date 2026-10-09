package com.wingman.agent

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject

/**
 * 主界面（A1，docs/android-agent-design.md §5.4；A2 投屏授权 §5.6）：
 * 服务器配置 + 启停 + 状态轮询（1s nativeStatus）+ 权限引导
 * （无障碍设置入口、投屏授权对话框）。
 * A3：开机自启开关（BootCompletedReceiver 的放行来源）+ 机型保活指引
 * （厂商 ROM 后台清理对策，详见 docs/guides/android-keep-alive.md）
 * + 受限设置引导（Android 13+ 侧载开箱失败最高来源，自动弹一次 +
 * 按钮常驻，详见 docs/guides/android-restricted-settings.md）。
 */
class MainActivity : AppCompatActivity() {

    private lateinit var prefs: android.content.SharedPreferences
    // serverToken 走 Keystore 加密库（A3-P2；装配时一次性迁移旧明文键）
    private lateinit var tokenStore: SecretStore
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
        tokenStore = SecretStores.tokenStore(this, prefs)

        val ipView = findViewById<EditText>(R.id.editServerIp)
        val portView = findViewById<EditText>(R.id.editServerPort)
        val agentView = findViewById<EditText>(R.id.editAgentId)
        val tokenView = findViewById<EditText>(R.id.editServerToken)

        ipView.setText(prefs.getString("serverIp", "192.168.1.10"))
        portView.setText(prefs.getInt("serverPort", 8888).toString())
        agentView.setText(prefs.getString("agentId", "android-" + android.os.Build.MODEL))
        tokenView.setText(tokenStore.read())

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

        // A3 部署体验：受限设置指引（Android 13+ 侧载默认屏蔽无障碍等受限
        // 服务——用户去系统设置打不开开关的根因；三档解法见对话框正文）
        findViewById<Button>(R.id.btnRestrictedSettingsGuide).setOnClickListener {
            showRestrictedSettingsGuide()
        }

        // A3：开机自启开关（默认关、显式开启——BootCompletedReceiver 放行
        // 判定读同一键；默认值单一来源 BootStartGate.DEFAULT_ENABLED）
        val bootView = findViewById<CheckBox>(R.id.checkAutoStartBoot)
        bootView.isChecked = prefs.getBoolean(AgentPrefs.KEY_AUTO_START_ON_BOOT, BootStartGate.DEFAULT_ENABLED)
        bootView.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(AgentPrefs.KEY_AUTO_START_ON_BOOT, checked).apply()
        }

        // A3-P2：challenge 注册鉴权开关（默认关 = P1 明文兼容——旧 server
        // 不理解 challenge 字段会按缺 token 拒绝）
        val challengeView = findViewById<CheckBox>(R.id.checkChallengeAuth)
        challengeView.isChecked = prefs.getBoolean(AgentPrefs.KEY_CHALLENGE_AUTH, false)
        challengeView.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean(AgentPrefs.KEY_CHALLENGE_AUTH, checked).apply()
        }

        // A3：机型保活指引（厂商 ROM 对策清单，完整版 docs/guides/android-keep-alive.md）
        findViewById<Button>(R.id.btnKeepAliveGuide).setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle(R.string.keep_alive_guide_title)
                .setMessage(R.string.keep_alive_guide_body)
                .setPositiveButton(android.R.string.ok, null)
                .show()
        }

        statusView = findViewById(R.id.textStatus)
    }

    override fun onResume() {
        super.onResume()
        uiHandler.post(pollStatus)
        maybePromptRestrictedSettings()
    }

    override fun onPause() {
        super.onPause()
        uiHandler.removeCallbacks(pollStatus)
    }

    // A3 部署体验：Android 13+ 且无障碍未启用时，回前台自动弹一次受限设置
    // 引导（判定口径与诚实边界见 RestrictedSettingsPolicy）。弹过一次即落
    // ack 不再自动弹；按钮入口常驻，可随时重看。
    private fun maybePromptRestrictedSettings() {
        if (!RestrictedSettingsPolicy.shouldPrompt(
                android.os.Build.VERSION.SDK_INT,
                isAccessibilityEnabled(),
                prefs.getBoolean(AgentPrefs.KEY_RESTRICTED_HINT_ACK, false),
            )
        ) {
            return
        }
        prefs.edit().putBoolean(AgentPrefs.KEY_RESTRICTED_HINT_ACK, true).apply()
        showRestrictedSettingsGuide()
    }

    private fun showRestrictedSettingsGuide() {
        AlertDialog.Builder(this)
            .setTitle(R.string.restricted_settings_guide_title)
            .setMessage(R.string.restricted_settings_guide_body)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    // 与 WingmanService 同款判定：公开 API 无「被受限设置挡住」状态，只能查
    // 已启用无障碍服务列表是否含本包。三行副本抽公共层需 Context 注入，
    // 不值得；两处同步维护。
    private fun isAccessibilityEnabled(): Boolean {
        val setting = android.provider.Settings.Secure.getString(
            contentResolver, android.provider.Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES)
        return setting?.contains(packageName) == true
    }

    private fun saveConfig(ipView: EditText, portView: EditText, agentView: EditText, tokenView: EditText) {
        prefs.edit()
            .putString("serverIp", ipView.text.toString().trim())
            .putInt("serverPort", portView.text.toString().trim().toIntOrNull() ?: 8888)
            .putString("agentId", agentView.text.toString().trim())
            .apply()
        // token 不落明文库，走 Keystore 加密库（A3-P2）
        tokenStore.write(tokenView.text.toString().trim())
        // A3-P2：challenge 开关随配置一并落库（WingmanService.startCore 读）
        prefs.edit()
            .putBoolean(AgentPrefs.KEY_CHALLENGE_AUTH,
                findViewById<CheckBox>(R.id.checkChallengeAuth).isChecked)
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
