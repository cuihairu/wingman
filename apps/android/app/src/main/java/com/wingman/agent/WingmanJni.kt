package com.wingman.agent

import android.accessibilityservice.AccessibilityService

/**
 * JNI 声明（A1，docs/android-agent-design.md §5.3；A2 反向桥 §5.6）。
 *
 * 窄接口：Kotlin → C++ 三个生命周期方法 + A2 反向回调三个方法；实现于
 * app/src/main/cpp/jni_bridge.cpp，背后是纯 C++ 核心 AndroidAgent。
 * 系统权限与 Android 生态的活全在 Kotlin；可移植的活（通信/脚本/日志/
 * 找色找图）全在 C++。
 */
object WingmanJni {
    init {
        System.loadLibrary("wingman_agent")
    }

    /** 启动 C++ 核心（出站长链接 + 命令分发）。已在跑时幂等返回 true。 */
    external fun nativeStart(configJson: String): Boolean

    /** 停止 C++ 核心（停脚本 → 断链路）。幂等。 */
    external fun nativeStop()

    /**
     * 状态 JSON：
     * {"running":bool,"connected":bool,"connectionState":str,
     *  "script":{"running":bool,"executionId":str}}
     */
    external fun nativeStatus(): String

    // ---------- A2 反向桥（C++ 宿主桥的 Kotlin 入口） ----------

    /**
     * 注册/注销无障碍服务桥（onServiceConnected 传 this，onDestroy 传 null）。
     * 必须在主线程调用（C++ 侧 GetObjectClass + GlobalRef + 缓存方法 ID）。
     */
    external fun nativeSetInputBridge(service: AccessibilityService?)

    /** 手势完成回调（GestureResultCallback）：seq 对应 performGesture 返回值。 */
    external fun nativeOnGestureResult(seq: Int, completed: Boolean)

    /**
     * 屏幕帧推送（ImageReader onImageAvailable，handler 线程）：
     * RGBA_8888 平面字节（含 rowStride 行距）；调用后 Image 即可 close，
     * C++ 侧拷贝进自己的缓存。
     */
    external fun nativeOnFrame(rgba: ByteArray, width: Int, height: Int, rowStride: Int)
}
