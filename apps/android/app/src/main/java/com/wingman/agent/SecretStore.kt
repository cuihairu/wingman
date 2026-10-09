package com.wingman.agent

import android.content.Context
import android.content.SharedPreferences
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKeys

/**
 * serverToken 存储抽象（A3-P2 安全演进：token 迁移 Android Keystore，
 * docs/agent-token-auth-design.md §4.3）。
 *
 * - [EncryptedSecretStore]：Keystore 主密钥 + EncryptedSharedPreferences，
 *   明文不再落普通 prefs（M1 形态，SharedPreferences 信任级别偏低）；
 * - [PlainSecretStore]：旧「wingman」prefs 明文键，仅作迁移来源；
 * - [migrateToken]：纯函数（JVM 单测覆盖），语义为「确保 secure 持有
 *   值后即清除明文残留」——legacy 非空时先迁移（secure 空才写入，
 *   不覆盖用户在 secure 侧的新值），随后无条件清 legacy。
 */
interface SecretStore {
    fun read(): String
    fun write(value: String)
    fun clear()
}

/** 旧版明文存储（迁移来源；JVM 测试以 fake 实现代替）。 */
class PlainSecretStore(private val prefs: SharedPreferences) : SecretStore {
    override fun read(): String = prefs.getString(AgentPrefs.KEY_SERVER_TOKEN, "") ?: ""
    override fun write(value: String) {
        prefs.edit().putString(AgentPrefs.KEY_SERVER_TOKEN, value).apply()
    }
    override fun clear() {
        prefs.edit().remove(AgentPrefs.KEY_SERVER_TOKEN).apply()
    }
}

/** Keystore 加密存储（真机验证项：首启生成主密钥，卸载即销毁）。 */
class EncryptedSecretStore(context: Context) : SecretStore {
    private val prefs: SharedPreferences = run {
        val masterKey = MasterKeys.getOrCreate(MasterKeys.AES256_GCM_SPEC)
        EncryptedSharedPreferences.create(
            "wingman_secure",
            masterKey,
            context,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }

    override fun read(): String = prefs.getString(AgentPrefs.KEY_SERVER_TOKEN, "") ?: ""
    override fun write(value: String) {
        prefs.edit().putString(AgentPrefs.KEY_SERVER_TOKEN, value).apply()
    }
    override fun clear() {
        prefs.edit().remove(AgentPrefs.KEY_SERVER_TOKEN).apply()
    }
}

object SecretStores {
    /**
     * 装配 token 存储：Keystore 加密库 + 一次性迁移旧明文键。
     * 幂等（secure 已有值时迁移为纯清理）；EncryptedSharedPreferences
     * 初始化失败时回退明文库（保持可用性，日志侧可见），与 P1 行为一致。
     */
    fun tokenStore(context: Context, prefs: SharedPreferences): SecretStore {
        val secure = try {
            EncryptedSecretStore(context)
        } catch (e: Exception) {
            android.util.Log.e("SecretStore", "EncryptedSharedPreferences unavailable, fallback to plain prefs", e)
            return PlainSecretStore(prefs)
        }
        val legacy = PlainSecretStore(prefs)
        migrateToken(legacy, secure)
        return secure
    }

    /** 迁移语义见文件头注释；对两个 store 的实现无框架依赖，可 JVM 单测。 */
    fun migrateToken(legacy: SecretStore, secure: SecretStore) {
        val value = legacy.read()
        if (value.isEmpty()) return
        if (secure.read().isEmpty()) {
            secure.write(value)
        }
        legacy.clear()
    }
}
