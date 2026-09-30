package com.wingman.agent

/**
 * 「wingman」SharedPreferences 的键单一来源（MainActivity/WingmanService/
 * CrashRestartHandler/BootCompletedReceiver 共用），避免字符串键漂移。
 */
object AgentPrefs {
    const val NAME = "wingman"

    // A1 配置面（MainActivity 写，WingmanService.startCore 读）
    const val KEY_SERVER_IP = "serverIp"
    const val KEY_SERVER_PORT = "serverPort"
    const val KEY_AGENT_ID = "agentId"
    const val KEY_SERVER_TOKEN = "serverToken"

    // A3 可靠性面
    const val KEY_AUTO_START_ON_BOOT = "autoStartOnBoot" // 默认 true（开关在 MainActivity）
    const val KEY_CORE_RUNNING = "coreRunning" // 崩溃时服务是否在跑（重启恢复的门控）
    const val KEY_CRASH_COUNT = "crashCount" // RestartPolicy.CrashState.crashCount
    const val KEY_CRASH_LAST_AT = "crashLastAt" // CrashState.lastCrashAtMs
    const val KEY_CRASH_WINDOW_START = "crashWindowStart" // CrashState.windowStartMs
    const val KEY_LAST_CRASH_MESSAGE = "lastCrashMessage" // 最近一次崩溃摘要（诊断用）

    // A3 部署体验面（受限设置引导，MainActivity 读写）
    const val KEY_RESTRICTED_HINT_ACK = "restrictedHintAck" // 自动引导弹过一次即置位
}
