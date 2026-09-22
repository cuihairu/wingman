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

set(ANDROIDAGENT_SOURCES
    src/script_runner.cpp
    src/android_script_api.cpp
    # lib/wingman cherry-pick 子集（A2；docs/android-agent-design.md §5.6）。
    # 桌面构建中 bitmap/screen/image_analyzer 与 wingman core 重复编译同一
    # TU——OpenCV 判定逻辑与 core 严格一致（下方同款防御），配置相同、
    # 链接器仅取其一；NDK 构建无 core 目标，全部由本目标提供。
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/bitmap.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/screen.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/vision/image_analyzer.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_host_bridge.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_capture.cpp"
)

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

# 视觉分支（screen.cpp fromFile/save 需 WINGMAN_ENABLE_VISION；
# android vcpkg 清单必含 opencv4，桌面按可用性自动降级）。
# 非 vcpkg 来源的 OpenCV 一律忽略——判定逻辑与 lib/wingman 保持一致，
# 确保重复编译的 TU 配置相同。
find_package(OpenCV CONFIG QUIET)
if(OpenCV_FOUND AND NOT OpenCV_DIR MATCHES "/vcpkg_installed/")
    message(STATUS "Ignoring non-vcpkg OpenCV at ${OpenCV_DIR}")
    set(OpenCV_FOUND FALSE)
endif()
if(OpenCV_FOUND AND TARGET opencv_core)
    target_compile_definitions(wingman_androidagent PRIVATE WINGMAN_ENABLE_VISION)
    target_link_libraries(wingman_androidagent PRIVATE
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
    )
endif()

target_link_libraries(wingman_androidagent PRIVATE
    sol2::sol2
    unofficial::lua::lua
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
)

target_compile_features(wingman_androidagent PUBLIC cxx_std_23)
