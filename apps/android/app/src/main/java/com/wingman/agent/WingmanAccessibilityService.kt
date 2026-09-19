package com.wingman.agent

import android.accessibilityservice.AccessibilityService
import android.view.accessibility.AccessibilityEvent

/**
 * 无障碍服务骨架（A2 落地，docs/android-agent-design.md §1.2）。
 *
 * A2 在此承接 Kotlin 侧 dispatchGesture 注入与 platform/android 租户的
 * IInput 桥接（经 JNI 窄接口追加 capture/inject 方法）。A1 只声明占位，
 * 让设备提前完成无障碍授权引导。
 */
class WingmanAccessibilityService : AccessibilityService() {

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {
        // A2: 事件消费与注入调度
    }

    override fun onInterrupt() {
        // A2
    }
}
