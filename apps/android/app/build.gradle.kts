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

        ndk {
            // A1 只出 arm64 真机；模拟器按需加 x86_64
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                // vcpkg 工具链与 NDK 工具链叠加（vcpkg 官方支持的组合，§6.2）
                arguments += listOf(
                    "-DCMAKE_TOOLCHAIN_FILE=$wingmanVcpkgRoot/scripts/buildsystems/vcpkg.cmake",
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
            version = "3.22.1"
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
