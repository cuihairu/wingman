package com.wingman.agent

import android.accessibilityservice.AccessibilityService
import android.accessibilityservice.GestureDescription
import android.graphics.Path
import android.os.Handler
import android.os.Looper
import android.view.accessibility.AccessibilityEvent

/**
 * 无障碍注入服务（A2 实装，docs/android-agent-design.md §5.6）。
 *
 * 承接 platform/android 宿主桥的注入半边：
 *  - onServiceConnected 时经 JNI 把自身注册进 C++（nativeSetInputBridge）；
 *  - C++ 脚本线程调用 performGesture（JNI CallIntMethod）→ 主线程
 *    dispatchGesture → GestureResultCallback 经 nativeOnGestureResult
 *    唤醒 C++ 侧等待（seq/promise，超时兜底在 C++）。
 *
 * dispatchGesture 要求主线程：非主线程调用时 post 到主 Handler。
 * A2 不消费无障碍事件（onAccessibilityEvent 保持空）。
 */
class WingmanAccessibilityService : AccessibilityService() {

    private val mainHandler = Handler(Looper.getMainLooper())
    private val gestureSeq = java.util.concurrent.atomic.AtomicInteger(1)

    /** C++ 宿主桥入口：返回手势 seq（seq 唯一，C++ 侧据此等待完成回调）。 */
    fun performGesture(x1: Int, y1: Int, x2: Int, y2: Int, durationMs: Int): Int {
        val seq = gestureSeq.getAndIncrement()
        if (Looper.myLooper() != Looper.getMainLooper()) {
            // C++ 脚本线程是常态调用方：dispatchGesture 要求主线程，
            // post 过去；dispatch 失败时以 completed=false 回调唤醒等待方
            mainHandler.post { dispatchOnMain(seq, x1, y1, x2, y2, durationMs) }
            return seq
        }
        return dispatchOnMain(seq, x1, y1, x2, y2, durationMs)
    }

    private fun dispatchOnMain(seq: Int, x1: Int, y1: Int, x2: Int, y2: Int,
                               durationMs: Int): Int {
        val duration = durationMs.coerceIn(1, 60_000)
        val path = Path().apply {
            moveTo(x1.toFloat(), y1.toFloat())
            if (x1 != x2 || y1 != y2) {
                lineTo(x2.toFloat(), y2.toFloat())
            }
        }
        val stroke = GestureDescription.StrokeDescription(path, 0L, duration.toLong())
        val gesture = GestureDescription.Builder().addStroke(stroke).build()
        val dispatched = dispatchGesture(gesture, object : GestureResultCallback() {
            override fun onCompleted(gestureDescription: GestureDescription?) {
                WingmanJni.nativeOnGestureResult(seq, true)
            }

            override fun onCancelled(gestureDescription: GestureDescription?) {
                WingmanJni.nativeOnGestureResult(seq, false)
            }
        }, mainHandler)
        if (!dispatched) {
            WingmanJni.nativeOnGestureResult(seq, false)
        }
        return seq
    }

    override fun onServiceConnected() {
        super.onServiceConnected()
        WingmanJni.nativeSetInputBridge(this)
    }

    override fun onUnbind(intent: android.content.Intent?): Boolean {
        WingmanJni.nativeSetInputBridge(null)
        return super.onUnbind(intent)
    }

    override fun onDestroy() {
        WingmanJni.nativeSetInputBridge(null)
        super.onDestroy()
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {
        // A2 不消费事件：仅使用 dispatchGesture 注入能力
    }

    override fun onInterrupt() {
        // 无事件流时的系统中断，与注入无关
    }
}
