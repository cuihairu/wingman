package com.wingman.agent

import android.app.Application

/**
 * A3：进程入口，安装进程级崩溃处理器（崩溃自重启路径，
 * docs/android-agent-design.md §7）。无其他初始化职责。
 */
class WingmanApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        CrashRestartHandler.install(this)
    }
}
