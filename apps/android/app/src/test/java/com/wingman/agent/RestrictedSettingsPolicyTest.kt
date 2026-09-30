package com.wingman.agent

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * RestrictedSettingsPolicy 纯逻辑直测（A3 部署体验）：API 门槛边界与
 * 提示矩阵。判定口径（无法区分「被挡」与「未开启」）见对象文档。
 */
class RestrictedSettingsPolicyTest {

    @Test
    fun affectedStartsExactlyAtApi33() {
        assertFalse(RestrictedSettingsPolicy.isAffected(31))
        assertFalse(RestrictedSettingsPolicy.isAffected(32))
        assertTrue(RestrictedSettingsPolicy.isAffected(33))
        assertTrue(RestrictedSettingsPolicy.isAffected(34))
    }

    @Test
    fun constantPinsAndroid13() {
        assertEquals(33, RestrictedSettingsPolicy.RESTRICTED_SETTINGS_API)
    }

    @Test
    fun promptsWhenAffectedAccessibilityOffAndNotAcked() {
        assertTrue(RestrictedSettingsPolicy.shouldPrompt(33, false, false))
        assertTrue(RestrictedSettingsPolicy.shouldPrompt(34, false, false))
    }

    @Test
    fun noPromptBelowApi33RegardlessOfOtherInputs() {
        assertFalse(RestrictedSettingsPolicy.shouldPrompt(32, false, false))
        assertFalse(RestrictedSettingsPolicy.shouldPrompt(31, false, false))
    }

    @Test
    fun noPromptWhenAccessibilityAlreadyEnabled() {
        assertFalse(RestrictedSettingsPolicy.shouldPrompt(33, true, false))
        assertFalse(RestrictedSettingsPolicy.shouldPrompt(34, true, false))
    }

    @Test
    fun noPromptAfterAcknowledged() {
        assertFalse(RestrictedSettingsPolicy.shouldPrompt(33, false, true))
        assertFalse(RestrictedSettingsPolicy.shouldPrompt(34, false, true))
    }
}
