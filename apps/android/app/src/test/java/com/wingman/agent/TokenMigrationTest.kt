package com.wingman.agent

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * A3-P2 token 迁移（SecretStores.migrateToken）JVM 单测：明文 → Keystore
 * 加密库的一次性迁移语义。两个 store 用 fake 实现（框架无关，纯逻辑验证）：
 * 「确保 secure 持有值后即清除明文残留」——不覆盖 secure 侧新值、无明文
 * 时不动、迁移后 legacy 必清。
 */
class TokenMigrationTest {

    private class FakeStore : SecretStore {
        var value: String = ""
        var cleared = false
        override fun read(): String = value
        override fun write(value: String) {
            this.value = value
            cleared = false
        }
        override fun clear() {
            value = ""
            cleared = true
        }
    }

    @Test
    fun migratesLegacyTokenToEmptySecureStore() {
        val legacy = FakeStore().apply { value = "wt_legacy" }
        val secure = FakeStore()
        SecretStores.migrateToken(legacy, secure)
        assertEquals("wt_legacy", secure.value)
        assertTrue(legacy.cleared)
        assertEquals("", legacy.value)
    }

    @Test
    fun doesNotOverwriteExistingSecureValue() {
        val legacy = FakeStore().apply { value = "wt_legacy" }
        val secure = FakeStore().apply { value = "wt_new" }
        SecretStores.migrateToken(legacy, secure)
        assertEquals("wt_new", secure.value)
        assertTrue(legacy.cleared)
    }

    @Test
    fun noLegacyTokenIsNoOp() {
        val legacy = FakeStore()
        val secure = FakeStore()
        SecretStores.migrateToken(legacy, secure)
        assertEquals("", secure.value)
        assertFalse(legacy.cleared)
        assertFalse(secure.cleared)
    }

    @Test
    fun emptyLegacyTokenIsNoOp() {
        val legacy = FakeStore().apply { value = "" }
        val secure = FakeStore()
        SecretStores.migrateToken(legacy, secure)
        assertEquals("", secure.value)
        assertFalse(legacy.cleared)
    }

    @Test
    fun migratedValueRoundtripsThroughStore() {
        val legacy = FakeStore().apply { value = "wt_roundtrip" }
        val secure = FakeStore()
        SecretStores.migrateToken(legacy, secure)
        assertEquals("wt_roundtrip", secure.read())
        secure.write("wt_roundtrip")
        assertEquals("wt_roundtrip", secure.read())
    }
}
