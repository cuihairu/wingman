plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// vcpkg 根（gradle.properties 的 wingmanVcpkgRoot，见 README「环境准备」）
val wingmanVcpkgRoot: String =
    (project.findProperty("wingmanVcpkgRoot") as String? ?: "").trimEnd('/')

android {
    namespace = "com.wingman.agent"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.wingman.agent"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0"

        // 显式对齐 CI runner 预装版本（AGP 默认 NDK 26.1 过旧，与最新 vcpkg
        // toolchain 的探测/组合行为不一致）
        ndkVersion = "27.3.13750724"

        ndk {
            // A1 只出 arm64 真机；模拟器按需加 x86_64
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                // vcpkg 工具链与 NDK 工具链叠加（§6.2）：经 cpp/vcpkg-android.cmake
                // 显式 chainload NDK android.toolchain.cmake——vcpkg 不自动加载
                // NDK 工具链，直接指向 vcpkg.cmake 会导致 ANDROID_* 变量失效
                arguments += listOf(
                    "-DCMAKE_TOOLCHAIN_FILE=" + file("../cpp/vcpkg-android.cmake").absolutePath,
                    "-DWINGMAN_VCPKG_ROOT=$wingmanVcpkgRoot",
                    "-DVCPKG_TARGET_TRIPLET=arm64-android",
                    "-DANDROID_STL=c++_static",
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            // C++ 工程位于仓库 apps/android/cpp（与 Kotlin 壳分层，见 §5.2）
            path = file("../cpp/CMakeLists.txt")
            // 3.22.1 的 File API reply 与 AGP 8.5 + 最新 vcpkg toolchain 组合
            // 触发 AGP readCmakeFileApiReply 解析失败，升到 SDK 3.31 组件
            version = "3.31.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
}
