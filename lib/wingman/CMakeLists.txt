cmake_minimum_required(VERSION 3.20)

# ========== wingman ==========
# 核心功能模块 - 输入输出、屏幕、窗口、触发器等

project(wingman LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(OpenCV CONFIG)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG)
find_package(unofficial-lua CONFIG)

# ========== 源文件 ==========
set(WINGMAN_SOURCES
    # 核心架构
    src/core/component.cpp
    src/core/event_bus.cpp

    # 视觉与捕获
    src/capture/capture_source.cpp
    src/vision/image_analyzer.cpp

    # 输入输出
    src/screen.cpp
    src/input.cpp
    src/window.cpp
    src/process.cpp
    src/human.cpp
    src/ui_automation.cpp

    # 触发器与视觉
    src/trigger.cpp
    src/trigger_engine.cpp
    src/smart_trigger.cpp
    src/vision.cpp

    # 系统模块
    src/system.cpp
    src/performance.cpp
    # src/security.cpp  # TODO: 修复重复定义

    # 账号与存储
    # src/accounts.cpp  # TODO: 检查依赖
    # src/storage.cpp  # TODO: 检查依赖
    # src/auth.cpp  # TODO: 检查依赖

    # 配置与工具
    src/config.cpp
    src/recorder.cpp
    src/json.cpp
    src/kvstore.cpp

    # 网络与通信
    src/http.cpp
    src/remote_server.cpp
    src/screenshot_reporter.cpp
    # src/http_server.cpp  # TODO: 检查依赖
    # src/node_status.cpp  # TODO: 检查依赖
    # src/storage_remote.cpp  # TODO: 检查依赖

    # 其他功能
    # src/qrcode.cpp  # TODO: 检查依赖
    # src/verification.cpp  # TODO: 检查依赖
    # src/behavior_tree.cpp  # TODO: 检查依赖
    # src/game_profile.cpp  # TODO: 检查依赖
    src/tray.cpp
)

# OCR 支持（可选）
if(DEFINED WINGMAN_ENABLE_OCR AND WINGMAN_ENABLE_OCR)
    list(APPEND WINGMAN_SOURCES src/ocr.cpp)
    add_compile_definitions(WINGMAN_ENABLE_OCR)
else()
    # 使用 stub 实现
    list(APPEND WINGMAN_SOURCES src/ocr_stub.cpp)
endif()

# ML 支持（可选）
if(DEFINED WINGMAN_ENABLE_ML AND WINGMAN_ENABLE_ML)
    list(APPEND WINGMAN_SOURCES src/ml.cpp)
    add_compile_definitions(WINGMAN_ENABLE_ML)
else()
    # 使用 stub 实现
    list(APPEND WINGMAN_SOURCES src/ml_stub.cpp)
endif()

# ========== 平台特定源文件 ==========
if(WIN32)
    list(APPEND WINGMAN_SOURCES
        src/platform/win/gdi_capture.cpp
        src/platform/win/sendinput_input.cpp
        src/platform/win/win32_window.cpp
        src/platform/win/win32_screen.cpp
        src/platform/win/win_automation.cpp
    )
elseif(APPLE)
    list(APPEND WINGMAN_SOURCES
        src/platform/mac/sccapture_capture.cpp
        src/platform/mac/cgevent_input.cpp
        src/platform/mac/cocoa_window.cpp
        src/platform/mac/cocoa_screen.cpp
        src/platform/mac/mac_automation.cpp
    )
    # macOS 需要 Cocoa 框架
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(SCREENCAPTUREKIT_FRAMEWORK ScreenCaptureKit)
endif()

# ========== 创建库 ==========
find_package(Threads REQUIRED)
add_library(wingman STATIC ${WINGMAN_SOURCES})
target_link_libraries(wingman PRIVATE Threads::Threads)

# 调试信息
message(STATUS "WINGMAN_GENERATED_DIR in wingman: ${WINGMAN_GENERATED_DIR}")

# 包含目录
target_include_directories(wingman PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
# 添加生成的头文件目录 (不使用生成器表达式)
target_include_directories(wingman PUBLIC ${WINGMAN_GENERATED_DIR})

# 链接库
set(WINGMAN_LINK_LIBS
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# 添加 OpenCV（如果可用）
if(TARGET opencv_core)
    list(APPEND WINGMAN_LINK_LIBS opencv_core opencv_imgproc opencv_imgcodecs)
endif()

# 添加 sqlite3（如果可用）
if(TARGET unofficial::sqlite3::sqlite3)
    list(APPEND WINGMAN_LINK_LIBS unofficial::sqlite3::sqlite3)
endif()

# 添加 Lua（如果可用）
if(TARGET unofficial::lua::lua)
    list(APPEND WINGMAN_LINK_LIBS unofficial::lua::lua)
endif()

target_link_libraries(wingman PUBLIC ${WINGMAN_LINK_LIBS})

# macOS 特定框架
if(APPLE)
    target_link_libraries(wingman PUBLIC
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreGraphics"
    )
    if(SCREENCAPTUREKIT_FRAMEWORK)
        target_link_libraries(wingman PUBLIC "-framework ScreenCaptureKit")
        target_compile_definitions(wingman PUBLIC HAVE_SCREENCAPTUREKIT)
    endif()
endif()
# spdlog 使用 header-only，不传递链接依赖
target_link_libraries(wingman PRIVATE spdlog::spdlog_header_only)

# Windows 特定库
if(WIN32)
    target_link_libraries(wingman PUBLIC
        gdiplus
        user32
        gdi32
        comdlg32
    )
    target_compile_definitions(wingman PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 导出库
add_library(wingman::wingman ALIAS wingman)

# ========== 编译选项 ==========
target_compile_features(wingman PUBLIC cxx_std_17)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman PRIVATE /W4 /utf-8)
else()
    target_compile_options(wingman PRIVATE -Wall -Wextra -Wpedantic)
endif()
