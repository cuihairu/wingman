# ========== wingman::androidagent ==========
#
# Android agent 共用子集：script_runner + android_script_api + lib/wingman
# cherry-pick 清单（全仓库唯一，桌面单测与 NDK 构建同源编译）。
#
# 消费方：
#   - apps/android/cpp（NDK，libwingman_agent.so）
#   - libs/lua/tests（桌面同源单测：script_runner_test / android_api_test）
#
# lib/wingman 本体不进入 NDK 构建（规避 find_package(OpenSSL REQUIRED) 等
# 重量依赖），视觉/位图/租户子集由本目标收口；桌面构建中 bitmap /
# image_analyzer 与 wingman core 重复编译同一 TU（配置一致，链接器仅取其一），
# screen.cpp 仅在 core 目标缺席时由本目标提供。

cmake_minimum_required(VERSION 3.20)
project(wingman_androidagent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 仓库根（缺省按本目录两层上级推算；gradle 侧亦显式传入）
if(NOT WINGMAN_REPO_ROOT)
    get_filename_component(WINGMAN_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

find_package(sol2 CONFIG REQUIRED)
find_package(unofficial-lua CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# vision provider 通路依赖（aiSetup/aiLocate，AI 视觉识别节）：HTTP=libcurl
# （http.cpp 的 WINGMAN_HAS_CURL 实现）、凭据解密=AES-256-GCM（crypt.cpp）。
# android vcpkg 清单必含 curl/openssl（apps/android/cpp/vcpkg.json），
# 缺失=构建配置坏——明确报错，不静默降级。
find_package(CURL CONFIG QUIET)
find_package(OpenSSL CONFIG QUIET)
if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR "wingman_androidagent: curl 未找到（vision aiLocate HTTP 必需；检查 apps/android/cpp/vcpkg.json 与 vcpkg toolchain）")
endif()
if(NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR "wingman_androidagent: openssl 未找到（aiSetup apiKeyEnc 解密必需；检查 apps/android/cpp/vcpkg.json 与 vcpkg toolchain）")
endif()

# 视觉分支（screen.cpp fromFile/save 需 WINGMAN_ENABLE_VISION；
# android vcpkg 清单必含 opencv4，桌面按可用性自动降级）。
# 非 vcpkg 来源的 OpenCV 一律忽略——判定逻辑与 lib/wingman 保持一致，
# 确保重复编译的 TU 配置相同。
find_package(OpenCV CONFIG QUIET)
if(OpenCV_FOUND AND NOT OpenCV_DIR MATCHES "/vcpkg_installed/" AND NOT (_VCPKG_INSTALLED_DIR AND OpenCV_DIR MATCHES "^${_VCPKG_INSTALLED_DIR}/"))
    message(STATUS "Ignoring non-vcpkg OpenCV at ${OpenCV_DIR}")
    set(OpenCV_FOUND FALSE)
endif()
if(OpenCV_FOUND AND TARGET opencv_core)
    set(WINGMAN_ANDROIDAGENT_VISION TRUE)
else()
    set(WINGMAN_ANDROIDAGENT_VISION FALSE)
endif()

set(ANDROIDAGENT_SOURCES
    src/script_runner.cpp
    src/android_script_api.cpp
    # lib/wingman cherry-pick 子集（A2；docs/android-agent-design.md §5.6）。
    # 桌面构建中 bitmap/screen/image_analyzer 与 wingman core 重复编译同一
    # TU——OpenCV 判定逻辑与 core 严格一致（上方同款防御），配置相同、
    # 链接器仅取其一；NDK 构建无 core 目标，全部由本目标提供。
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/bitmap.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/screen.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/vision/image_analyzer.cpp"
    # AI 视觉 provider 通路：HTTP 发射（WINGMAN_HAS_CURL）与凭据解密
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/http.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/crypt.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_host_bridge.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_capture.cpp"
)
if(WINGMAN_ANDROIDAGENT_VISION)
    list(APPEND ANDROIDAGENT_SOURCES
        "${WINGMAN_REPO_ROOT}/lib/wingman/src/vision_ai.cpp")
else()
    # 与 lib/wingman 同构：无 OpenCV 编 VisionAi 假体（stub 语义：locate
    # 恒空 box + warn），aiSetup/aiLocate 绑定保持可注册
    list(APPEND ANDROIDAGENT_SOURCES
        "${WINGMAN_REPO_ROOT}/lib/wingman/src/vision_stub.cpp")
endif()

if(ANDROID)
    # Screen::capture 的 NDK 装配符号（公共层平台分区无 __ANDROID__ 段，
    # 边界守卫冻结；租户 TU 反向 include 公共头提供，桌面不编此 TU）
    list(APPEND ANDROIDAGENT_SOURCES
        "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_screen.cpp")
endif()

add_library(wingman_androidagent STATIC ${ANDROIDAGENT_SOURCES})
add_library(wingman::androidagent ALIAS wingman_androidagent)

target_include_directories(wingman_androidagent PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # wingman 公共头（screen.hpp / vision/image_analyzer.hpp）
    $<BUILD_INTERFACE:${WINGMAN_REPO_ROOT}/lib/wingman/include>
    # 平台租户私有头（platform/android/*.hpp）统一经本目标对外暴露；
    # 消费方不得自行向 lib/wingman/src 添加 include 路径
    $<BUILD_INTERFACE:${WINGMAN_REPO_ROOT}/lib/wingman/src>
)

if(WINGMAN_ANDROIDAGENT_VISION)
    # PUBLIC：aiSetup/aiLocate 绑定随本宏存在与否行为不同，
    # 消费方（android_api_test 分档断言）需可见
    target_compile_definitions(wingman_androidagent PUBLIC WINGMAN_ENABLE_VISION)
    target_link_libraries(wingman_androidagent PRIVATE
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
    )
endif()

# http.cpp 的 curl 实现段开关（与 lib/wingman 同款 target 级定义）
target_compile_definitions(wingman_androidagent PRIVATE WINGMAN_HAS_CURL)
target_link_libraries(wingman_androidagent PRIVATE
    CURL::libcurl
    OpenSSL::Crypto
)

target_link_libraries(wingman_androidagent PRIVATE
    sol2::sol2
    unofficial::lua::lua
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
)

target_compile_features(wingman_androidagent PUBLIC cxx_std_23)
