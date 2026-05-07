cmake_minimum_required(VERSION 3.20)

# ========== wingman-core ==========
# 核心功能模块 - 输入输出、屏幕、窗口、触发器等

project(wingman_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(OpenCV CONFIG)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG)

# ========== 源文件 ==========
set(CORE_SOURCES
    # 输入输出
    src/screen.cpp
    src/input.cpp
    src/window.cpp
    src/process.cpp
    src/human.cpp
    src/ui_automation.cpp

    # 触发器与视觉
    src/trigger.cpp
    src/smart_trigger.cpp
    src/vision.cpp

    # 系统模块
    src/system.cpp
    src/performance.cpp
    src/security.cpp

    # 账号与存储
    src/accounts.cpp
    src/storage.cpp
    src/auth.cpp

    # 配置与工具
    src/config.cpp
    src/recorder.cpp
    src/json.cpp
    src/kvstore.cpp

    # 网络与通信
    src/http.cpp
    src/node_status.cpp

    # 其他功能
    src/qrcode.cpp
    src/verification.cpp
    src/behavior_tree.cpp
    src/game_profile.cpp
    src/tray.cpp
)

# OCR 支持（可选）
if(DEFINED WINGMAN_ENABLE_OCR AND WINGMAN_ENABLE_OCR)
    list(APPEND CORE_SOURCES src/ocr.cpp)
    add_compile_definitions(WINGMAN_ENABLE_OCR)
else()
    # 使用 stub 实现
    list(APPEND CORE_SOURCES src/ocr_stub.cpp)
endif()

# ML 支持（可选）
if(DEFINED WINGMAN_ENABLE_ML AND WINGMAN_ENABLE_ML)
    list(APPEND CORE_SOURCES src/ml.cpp)
    add_compile_definitions(WINGMAN_ENABLE_ML)
else()
    # 使用 stub 实现
    list(APPEND CORE_SOURCES src/ml_stub.cpp)
endif()

# ========== 创建库 ==========
add_library(wingman-core STATIC ${CORE_SOURCES})

# 包含目录
target_include_directories(wingman-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 链接库
set(CORE_LINK_LIBS
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# 添加 OpenCV（如果可用）
if(TARGET opencv_core)
    list(APPEND CORE_LINK_LIBS opencv_core opencv_imgproc opencv_imgcodecs)
endif()

# 添加 sqlite3（如果可用）
if(TARGET unofficial::sqlite3::sqlite3)
    list(APPEND CORE_LINK_LIBS unofficial::sqlite3::sqlite3)
endif()

target_link_libraries(wingman-core PUBLIC ${CORE_LINK_LIBS})

# Windows 特定库
if(WIN32)
    target_link_libraries(wingman-core PUBLIC
        gdiplus
        user32
        gdi32
        gdiplus
        comdlg32
    )
endif()

# 导出库
add_library(wingman::core ALIAS wingman-core)

# ========== 编译选项 ==========
target_compile_features(wingman-core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(wingman-core PRIVATE /W4 /utf-8)
else()
    target_compile_options(wingman-core PRIVATE -Wall -Wextra -Wpedantic)
endif()
