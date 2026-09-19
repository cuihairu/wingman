// platform/android 租户占位（A1，docs/android-agent-design.md §5.5）
//
// A1 只建立租户目录与构建分支（本文件零依赖、任何编译器可通过），
// 保证 lib/wingman 的 Android 目标可配置；真实能力在 A2 按接口逐个落地：
//
//   IInput（注入）      ← Kotlin dispatchGesture，经 JNI 窄接口进翻译层
//   ICaptureSource      ← MediaProjection，经同上（位图经 JNI 直拷）
//   窗口/剪贴板/进程等   ← 按 Android 可用面评估，无对应能力则保持不可用
//
// A2 时删除本文件，替换为 android_input.cpp / android_capture.cpp 等
// 与 win/mac/linux 租户同构的实现文件（CMake ANDROID 分支同步扩充）。

namespace wingman::android {

// 租户版本标记：A2 实装时移除。链接期可引用，防止空翻译单元。
int androidTenantPlaceholder() {
    return 1;
}

} // namespace wingman::android
