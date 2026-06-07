cmake_minimum_required(VERSION 3.20)

# ========== wingman ==========
# Core functionality module - I/O, screen, window, triggers, etc.

if(APPLE)
    project(wingman LANGUAGES CXX OBJCXX)
else()
    project(wingman LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(OpenCV CONFIG QUIET)
if(OpenCV_FOUND)
    if(NOT OpenCV_DIR MATCHES "/vcpkg_installed/")
        message(STATUS "Ignoring non-vcpkg OpenCV at ${OpenCV_DIR}")
        set(OpenCV_FOUND FALSE)
    endif()
endif()
# Propagate OpenCV availability to sibling subdirectories (e.g., tests)
set(WINGMAN_HAS_OPENCV ${OpenCV_FOUND} CACHE INTERNAL "OpenCV available for wingman")
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL QUIET)
find_package(OpenSSL QUIET)
find_package(unofficial-sqlite3 CONFIG)
if(WINGMAN_BUILD_LUA)
    find_package(unofficial-lua CONFIG)
endif()

# ========== Source Files ==========
set(WINGMAN_SOURCES
    # Core architecture
    src/core/component.cpp
    src/core/event_bus.cpp
    src/event.cpp

    # Vision and capture
    src/capture/capture_source.cpp
    src/vision/image_analyzer.cpp

    # I/O
    src/screen.cpp
    src/window.cpp
    src/ui_automation.cpp
    src/clipboard.cpp
    src/filewatcher.cpp

    # Triggers
    src/trigger_engine.cpp

    # System modules (note: performance.cpp needs OpenCV, added later)
    src/security.cpp
    src/crypt.cpp

    # Storage
    src/storage.cpp

    # Configuration and utilities
    src/config.cpp
    src/json.cpp
    src/kvstore.cpp

    # IPC communication
    src/ipc/ipc_factory.cpp
    src/ipc/tcp_channel.cpp
    src/ipc/unix_socket_channel.cpp

    # Network
    src/http.cpp
    src/node_status.cpp

    # TOTP utilities
    src/verification.cpp
    src/behavior_tree.cpp
    src/game_profile.cpp
    src/human.cpp
    src/smart_trigger.cpp

    # Script abstraction layer (language-agnostic module descriptors)
    src/script_manager.cpp
    src/script/modules/screen_module.cpp
    src/script/modules/input_module.cpp
    src/script/modules/window_module.cpp
    src/script/modules/process_module.cpp
    src/script/modules/util_module.cpp
    src/script/modules/system_module.cpp
    src/script/modules/security_module.cpp
    src/script/modules/crypto_module.cpp
    src/script/modules/performance_module.cpp
    src/script/modules/game_profile_module.cpp
    src/script/modules/vision_module.cpp
    src/script/modules/misc_modules.cpp
    src/script/modules/config_module.cpp
    src/script/modules/verification_module.cpp
    src/script/modules/human_module.cpp
    src/script/modules/debugger_module.cpp
    src/script/modules/http_module.cpp
    src/script/modules/transport_module.cpp
    src/script/modules/json_module.cpp
    src/script/modules/kv_module.cpp
    src/script/modules/db_module.cpp
    src/script/modules/ini_module.cpp
    src/script/modules/event_module.cpp
    src/script/modules/inbox_module.cpp
    src/script/modules/team_module.cpp
    src/script/modules/fsm_module.cpp
    src/script/modules/task_module.cpp
    src/script/modules/notify_module.cpp
    src/script/modules/orchestration_module.cpp
    src/script/modules/module_registry.cpp
    src/platform/input_factory.cpp
    src/platform/mock/mock_input.cpp

    # Transport-agnostic command dispatcher and handlers
    src/rpc/rpc_dispatcher.cpp
    src/rpc/handlers/trigger_handler.cpp
    src/rpc/handlers/system_handler.cpp
)

# OCR support (optional)
if(DEFINED WINGMAN_ENABLE_OCR AND WINGMAN_ENABLE_OCR)
    list(APPEND WINGMAN_SOURCES src/ocr.cpp)
    add_compile_definitions(WINGMAN_ENABLE_OCR)
else()
    # Use stub implementation
    list(APPEND WINGMAN_SOURCES src/ocr_stub.cpp)
endif()

# ML support (optional)
if(DEFINED WINGMAN_ENABLE_ML AND WINGMAN_ENABLE_ML)
    list(APPEND WINGMAN_SOURCES src/ml.cpp)
    add_compile_definitions(WINGMAN_ENABLE_ML)
else()
    # Use stub implementation
    list(APPEND WINGMAN_SOURCES src/ml_stub.cpp)
endif()

# Vision support (optional, requires vcpkg OpenCV)
if(OpenCV_FOUND AND TARGET opencv_core)
    list(APPEND WINGMAN_SOURCES
        src/vision.cpp
        src/performance.cpp
    )
    add_compile_definitions(WINGMAN_ENABLE_VISION)
    message(STATUS "Vision support enabled (vcpkg OpenCV found)")
else()
    # Use stub implementation
    list(APPEND WINGMAN_SOURCES src/vision_stub.cpp)
    message(STATUS "Vision support disabled (vcpkg OpenCV not found)")
endif()

# ========== Platform-Specific Source Files ==========
if(WIN32)
    list(APPEND WINGMAN_SOURCES
        src/screenshot_reporter.cpp
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
    )
elseif(APPLE)
    list(APPEND WINGMAN_SOURCES
        src/platform/mac/cgevent_input.cpp
        src/platform/mac/cocoa_window.cpp
        src/platform/mac/cocoa_screen.cpp
        src/platform/mac/mac_automation.cpp
        src/platform/mac/posix_process.cpp
        src/platform/mac/darwin_system.cpp
        src/platform/mac/cocoa_recorder.cpp
        src/platform/mac/sccapture_capture.cpp
        src/platform/mac/cocoa_clipboard.cpp
        src/platform/mac/fsevents_filewatcher.cpp
        src/platform/unix/posix_trigger.cpp
    )
    set_source_files_properties(
        src/platform/mac/cgevent_input.cpp
        src/platform/mac/cocoa_window.cpp
        src/platform/mac/cocoa_screen.cpp
        src/platform/mac/mac_automation.cpp
        src/platform/mac/cocoa_recorder.cpp
        src/platform/mac/sccapture_capture.cpp
        src/platform/mac/cocoa_clipboard.cpp
        src/platform/mac/fsevents_filewatcher.cpp
        PROPERTIES LANGUAGE OBJCXX
    )
    # macOS requires Cocoa framework
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
    )
    # Linux requires X11 and XRandR
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(XRANDR REQUIRED xrandr)
    pkg_check_modules(XEXT REQUIRED xext)
    pkg_check_modules(XTST REQUIRED xtst)
endif()

# ========== Create Library ==========
find_package(Threads REQUIRED)
add_library(wingman STATIC ${WINGMAN_SOURCES})
target_link_libraries(wingman PRIVATE Threads::Threads)

# Debug info
message(STATUS "WINGMAN_GENERATED_DIR in wingman: ${WINGMAN_GENERATED_DIR}")

# Include directories
target_include_directories(wingman PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
# Add generated header file directory (without generator expression)
target_include_directories(wingman PUBLIC ${WINGMAN_GENERATED_DIR})

# Link libraries
set(WINGMAN_LINK_LIBS
    nlohmann_json::nlohmann_json
)

# Add OpenCV (if available)
if(OpenCV_FOUND AND TARGET opencv_core)
    list(APPEND WINGMAN_LINK_LIBS opencv_core opencv_imgproc opencv_imgcodecs)
endif()

# Add CURL (if available)
if(TARGET CURL::libcurl)
    list(APPEND WINGMAN_LINK_LIBS CURL::libcurl)
    target_compile_definitions(wingman PRIVATE WINGMAN_HAS_CURL)
    message(STATUS "HTTP support enabled (CURL found)")
else()
    message(STATUS "HTTP support disabled (CURL not found)")
endif()

if(TARGET OpenSSL::Crypto)
    list(APPEND WINGMAN_LINK_LIBS OpenSSL::Crypto)
endif()

# Add sqlite3 (if available)
if(TARGET unofficial::sqlite3::sqlite3)
    list(APPEND WINGMAN_LINK_LIBS unofficial::sqlite3::sqlite3)
    target_compile_definitions(wingman PUBLIC WINGMAN_HAS_SQLITE)
endif()

# Add Lua (if available)
if(WINGMAN_BUILD_LUA AND TARGET unofficial::lua::lua)
    list(APPEND WINGMAN_LINK_LIBS unofficial::lua::lua)
    target_compile_definitions(wingman PUBLIC WINGMAN_HAS_LUA)
endif()

target_link_libraries(wingman PUBLIC ${WINGMAN_LINK_LIBS})

# macOS-specific frameworks
if(APPLE)
    target_link_libraries(wingman PUBLIC
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreGraphics"
        "-framework ImageIO"
    )
    if(SCREENCAPTUREKIT_FRAMEWORK)
        target_link_libraries(wingman PUBLIC "-framework ScreenCaptureKit")
        target_compile_definitions(wingman PUBLIC HAVE_SCREENCAPTUREKIT)
    endif()
endif()
# spdlog uses header-only, no transitive link dependencies
target_link_libraries(wingman PRIVATE spdlog::spdlog_header_only)

# OpenSSL for AES-256-GCM encryption
find_package(OpenSSL REQUIRED)
target_link_libraries(wingman PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# hiredis for Redis client (remote events)
find_package(hiredis CONFIG QUIET)
if(hiredis_FOUND)
    target_link_libraries(wingman PUBLIC hiredis::hiredis)
    target_compile_definitions(wingman PUBLIC WINGMAN_HAS_HIREDIS)
    message(STATUS "Wingman: remote events enabled (hiredis)")
else()
    message(STATUS "Wingman: remote events disabled (hiredis not found)")
endif()

# Transport library for networking (TCP/UDP)
if(TARGET wingman::transport)
    target_link_libraries(wingman PUBLIC wingman::transport)
    target_compile_definitions(wingman PUBLIC WINGMAN_HAS_TRANSPORT)
    message(STATUS "Wingman: linking wingman::transport")
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(wingman PUBLIC
        gdiplus
        user32
        gdi32
        comdlg32
        ws2_32
    )
    target_compile_definitions(wingman PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Linux-specific libraries
if(UNIX AND NOT APPLE)
    target_include_directories(wingman PRIVATE ${X11_INCLUDE_DIRS} ${XRANDR_INCLUDE_DIRS} ${XEXT_INCLUDE_DIRS} ${XTST_INCLUDE_DIRS})
    target_link_libraries(wingman PUBLIC ${X11_LIBRARIES} ${XRANDR_LIBRARIES} ${XEXT_LIBRARIES} ${XTST_LIBRARIES})
    target_compile_definitions(wingman PUBLIC LINUX_PLATFORM)
endif()

# Export library
add_library(wingman::wingman ALIAS wingman)

# ========== Compile Options ==========
target_compile_features(wingman PUBLIC cxx_std_23)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman PRIVATE /W4 /utf-8)
else()
    target_compile_options(wingman PRIVATE -Wall -Wextra -Wpedantic)
endif()
