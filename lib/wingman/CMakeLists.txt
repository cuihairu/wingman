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
    src/ui_automation.cpp
    src/clipboard.cpp
    src/filewatcher.cpp

    # 触发器与视觉
    src/trigger_engine.cpp

    # 系统模块（注意：performance.cpp 需要 OpenCV，在后面添加）
    src/security.cpp

    # 账号与存储
    # src/accounts.cpp  # TODO: 检查依赖
    # src/storage.cpp  # TODO: 检查依赖
    # src/auth.cpp  # TODO: 检查依赖

    # 配置与工具
    src/config.cpp
    src/json.cpp
    src/kvstore.cpp

    # IPC 通信
    src/ipc/ipc_factory.cpp

    # 网络与通信
    src/http.cpp
    src/screenshot_reporter.cpp
    # src/http_server.cpp  # TODO: 检查依赖
    # src/node_status.cpp  # TODO: 检查依赖
    # src/storage_remote.cpp  # TODO: 检查依赖

    # 其他功能
    src/qrcode.cpp
    # src/verification.cpp  # TODO: 检查依赖
    # src/behavior_tree.cpp  # TODO: 检查依赖
    src/game_profile.cpp

    # 脚本抽象层（语言无关的模块描述符）
    src/script_manager.cpp
    src/script/modules/screen_module.cpp
    src/script/modules/input_module.cpp
    src/script/modules/window_module.cpp
    src/script/modules/process_module.cpp
    src/script/modules/util_module.cpp
    src/script/modules/system_module.cpp
    src/script/modules/security_module.cpp
    src/script/modules/performance_module.cpp
    src/script/modules/game_profile_module.cpp
    src/script/modules/vision_module.cpp
    src/script/modules/misc_modules.cpp
    src/script/modules/config_module.cpp
    src/script/modules/verification_module.cpp
    src/script/modules/human_module.cpp
    src/script/modules/debugger_module.cpp
    src/script/modules/http_module.cpp
    src/script/modules/json_module.cpp
    src/script/modules/kv_module.cpp
    src/script/modules/orchestration_module.cpp
    src/script/modules/module_registry.cpp
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

# Vision 支持（可选，需要 OpenCV）
if(TARGET opencv_core)
    list(APPEND WINGMAN_SOURCES
        src/vision.cpp
        src/performance.cpp
        src/platform/remote_server_common.cpp
    )
    add_compile_definitions(WINGMAN_ENABLE_VISION)
    message(STATUS "Vision support enabled (OpenCV found)")
else()
    # 使用 stub 实现
    list(APPEND WINGMAN_SOURCES src/vision_stub.cpp)
    message(STATUS "Vision support disabled (OpenCV not found)")
endif()

# ========== 平台特定源文件 ==========
if(WIN32)
    list(APPEND WINGMAN_SOURCES
        src/human.cpp
        src/smart_trigger.cpp
        src/platform/win/win32_clipboard.cpp
        src/platform/win/win32_filewatcher.cpp
        src/platform/win/named_pipe_channel.cpp
        src/platform/win/gdi_capture.cpp
        src/platform/win/sendinput_input.cpp
        src/platform/win/win32_window.cpp
        src/platform/win/win32_screen.cpp
        src/platform/win/win_automation.cpp
        src/platform/win/win32_process.cpp
        src/platform/win/win32_system.cpp
        src/platform/win/win32_recorder.cpp
        src/platform/win/win32_trigger.cpp
        src/platform/win/winsock_remote_server.cpp
    )
elseif(APPLE)
    list(APPEND WINGMAN_SOURCES
        src/platform/mac/sccapture_capture.cpp
        src/platform/mac/cgevent_input.cpp
        src/platform/mac/cocoa_window.cpp
        src/platform/mac/cocoa_screen.cpp
        src/platform/mac/mac_automation.cpp
        src/platform/mac/posix_process.cpp
        src/platform/mac/darwin_system.cpp
        src/platform/mac/cocoa_recorder.cpp
        src/platform/unix/posix_trigger.cpp
        src/platform/unix/berkeley_remote_server.cpp
    )
    # macOS 需要 Cocoa 框架
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(SCREENCAPTUREKIT_FRAMEWORK ScreenCaptureKit)
elseif(UNIX AND NOT APPLE)
    list(APPEND WINGMAN_SOURCES
        src/platform/linux/x11_factory.cpp
        src/platform/linux/posix_process.cpp
        src/platform/linux/posix_system.cpp
        src/platform/linux/x11_recorder.cpp
        src/platform/unix/posix_trigger.cpp
        src/platform/unix/berkeley_remote_server.cpp
    )
    # Linux 需要 X11 和 XRandR
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(XRANDR REQUIRED xrandr)
    pkg_check_modules(XEXT REQUIRED xext)
    pkg_check_modules(XTST REQUIRED xtst)
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

# Linux 特定库
if(UNIX AND NOT APPLE)
    target_include_directories(wingman PRIVATE ${X11_INCLUDE_DIRS} ${XRANDR_INCLUDE_DIRS} ${XEXT_INCLUDE_DIRS} ${XTST_INCLUDE_DIRS})
    target_link_libraries(wingman PUBLIC ${X11_LIBRARIES} ${XRANDR_LIBRARIES} ${XEXT_LIBRARIES} ${XTST_LIBRARIES})
    target_compile_definitions(wingman PUBLIC LINUX_PLATFORM)
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
