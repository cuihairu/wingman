// Wingman Android Agent 根构建脚本（A1，docs/android-agent-design.md §6）
plugins {
    // AGP 8.7.3：8.5.x 的 readCmakeFileApiReply 对 vcpkg toolchain 生成的
    // File API reply 存在解析崩溃（NPE），8.7 起该路径重写（docs §6.2）
    id("com.android.application") version "8.7.3" apply false
    id("org.jetbrains.kotlin.android") version "2.0.20" apply false
}
