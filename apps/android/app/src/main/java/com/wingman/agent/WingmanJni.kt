package com.wingman.agent

/**
 * JNI 声明（A1，docs/android-agent-design.md §5.3）。
 *
 * 窄接口：Kotlin → C++ 三个方法；实现于 app/src/main/cpp/jni_bridge.cpp，
 * 背后是纯 C++ 核心 AndroidAgent（RemoteClient + Lua）。系统权限与 Android
 * 生态的活全在 Kotlin；可移植的活（通信/脚本/日志）全在 C++。
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
}
