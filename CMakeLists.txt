cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用静态运行时库 (与 vcpkg x64-windows-static 一致)
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # 确保所有目标使用静态运行时库
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

# 使用 spdlog header-only 模式避免符号重复
add_compile_definitions(SPDLOG_HEADER_ONLY)

# Add UTF-8 compiler flag (MSVC only)
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(/utf-8)
endif()

# Add WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
add_compile_definitions(WIN32_LEAN_AND_MEAN)

# Add NOMINMAX to avoid min/max macro conflicts
add_compile_definitions(NOMINMAX)

# ========== vcpkg 集成 ==========
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# ========== 构建选项 ==========
option(WINGMAN_BUILD_SERVER "Build network server (legacy)" OFF)  # 默认关闭，使用新架构
option(WINGMAN_BUILD_DEBUGGER "Build debugger (legacy)" OFF)       # 默认关闭，使用 EmmyLua
option(WINGMAN_BUILD_TESTS "Build tests" OFF)
option(WINGMAN_BUILD_GUI "Build native GUI (requires ImGui)" ON)
option(WINGMAN_ENABLE_OCR "Enable OCR support (requires Tesseract)" OFF)  # 默认关闭
option(WINGMAN_ENABLE_ML "Enable ML/AI support (requires ONNX Runtime)" OFF)  # 默认关闭

# ========== 新架构选项 ==========
option(WINGMAN_BUILD_AGENT "Build new Agent (multi-module)" ON)
option(WINGMAN_ENABLE_EMMY "Enable EmmyLua Debugger integration" ON)

# ========== 测试选项 ==========
option(BUILD_CLIENT_TESTS "Build client tests" OFF)
option(BUILD_CORE_TESTS "Build core library tests" OFF)
option(BUILD_TRANSPORT_TESTS "Build transport library tests" OFF)
option(BUILD_LUA_TESTS "Build lua library tests" OFF)
option(BUILD_PROTO_TESTS "Build proto library tests" OFF)
option(BUILD_DEBUG_TESTS "Build debug library tests" OFF)

# ========== 核心依赖（必需）==========
# Windows-only 依赖（Linux/macOS 暂不支持）
if(WIN32)
    find_package(unofficial-lua CONFIG REQUIRED)
    find_package(asio CONFIG REQUIRED)
endif()
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(Threads REQUIRED)

# 设置 Lua 库变量（兼容性）
if(TARGET unofficial::lua::lua)
    set(LUA_LIBRARIES unofficial::lua::lua)
    set(LUA_INCLUDE_DIR "${unofficial-lua_DIR}/../../include")
endif()

# 可选依赖
find_package(OpenCV CONFIG)
find_package(unofficial-sqlite3 CONFIG)
find_package(Protobuf CONFIG)

# 测试依赖 - GTest 在各测试子目录中单独查找
# 这样可以避免在主配置阶段因找不到 GTest 而失败

# SQLite3 支持
add_compile_definitions(WINGMAN_HAS_SQLITE)
message(STATUS "SQLite3 persistence enabled")

# GUI 支持
if(WINGMAN_BUILD_GUI)
    find_package(imgui CONFIG QUIET)
    if(imgui_FOUND)
        add_compile_definitions(WINGMAN_BUILD_GUI)
        message(STATUS "Native GUI support enabled")
    else()
        message(WARNING "imgui not found, GUI support disabled")
    endif()
endif()

# OCR 支持
if(WINGMAN_ENABLE_OCR)
    add_compile_definitions(WINGMAN_ENABLE_OCR)
    message(STATUS "OCR support enabled")
endif()

# ML/AI 支持
if(WINGMAN_ENABLE_ML)
    add_compile_definitions(WINGMAN_ENABLE_ML)
    message(STATUS "ML/AI support enabled")
endif()

# EmmyLua 支持
if(WINGMAN_ENABLE_EMMY)
    add_compile_definitions(WINGMAN_ENABLE_EMMY)
    message(STATUS "EmmyLua Debugger enabled")
endif()

# ========== 版本信息 ==========
set(WINGMAN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/include" CACHE INTERNAL "Generated headers directory")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/wingman/include/wingman/version.hpp.in
    ${WINGMAN_GENERATED_DIR}/wingman/version.hpp
    @ONLY
)

# ========== 多模块架构 ==========

# 共享库子目录
if(WINGMAN_BUILD_AGENT)
    add_subdirectory(libs/proto)
    add_subdirectory(libs/transport)
    add_subdirectory(libs/lua)
    add_subdirectory(libs/debug)

    # 核心库
    add_subdirectory(lib/wingman)

    # 应用
    add_subdirectory(apps/runtime)
    # add_subdirectory(apps/inspector)  # 暂时禁用，需要 imgui

    message(STATUS "Multi-module build enabled")
endif()

# GUI 应用 (Tauri - 如果启用则构建)
if(WINGMAN_BUILD_GUI)
    add_subdirectory(apps/gui)
endif()

# ========== 测试（新架构）==========
if(WINGMAN_BUILD_TESTS)
    enable_testing()

    # 各模块测试（transport、lua、proto、debug 在各自模块内处理）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/wingman/tests/CMakeLists.txt")
        add_subdirectory(lib/wingman/tests)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/runtime/tests/CMakeLists.txt")
        add_subdirectory(apps/runtime/tests)
    endif()
endif()

# ========== 性能基准测试 ==========
option(WINGMAN_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
if(WINGMAN_BUILD_BENCHMARKS)
    add_subdirectory(lib/wingman/tests/benchmarks)
endif()

# ========== 安装 ==========
install(TARGETS wingman-runtime
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 复制脚本
install(DIRECTORY build-scripts/
    DESTINATION scripts
    FILES_MATCHING PATTERN "*.lua"
)

# 复制 VSCode 配置包
install(DIRECTORY assets/vscode-wingman-lua/
    DESTINATION vscode-config
)
