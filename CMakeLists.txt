cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
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
option(WINGMAN_BUILD_DEBUGGER "Build debugger (legacy)" OFF)       # 默认关闭，使用 EmmyLua
option(WINGMAN_BUILD_TESTS "Build tests" OFF)
option(WINGMAN_ENABLE_OCR "Enable OCR support (requires Tesseract)" OFF)  # 默认关闭
option(WINGMAN_ENABLE_ML "Enable ML/AI support (requires ONNX Runtime)" OFF)  # 默认关闭
option(WINGMAN_ENABLE_PYTHON "Enable Python scripting engine (requires CPython + pybind11)" OFF)
option(WINGMAN_COMPAT_BUILD "Build only portable compatibility targets" OFF)

# ========== 新架构选项 ==========
option(WINGMAN_BUILD_AGENT "Build new Agent (multi-module)" ON)
option(WINGMAN_ENABLE_EMMY "Enable EmmyLua Debugger integration" ON)
option(WINGMAN_BUILD_PROTO "Build protobuf module" ${WINGMAN_BUILD_AGENT})
option(WINGMAN_BUILD_TRANSPORT "Build transport module" ${WINGMAN_BUILD_AGENT})
option(WINGMAN_BUILD_CORE "Build core wingman library" ${WINGMAN_BUILD_AGENT})
option(WINGMAN_BUILD_LUA "Build Lua module" ${WINGMAN_BUILD_AGENT})
option(WINGMAN_BUILD_DEBUG "Build debug module" ${WINGMAN_BUILD_AGENT})
option(WINGMAN_BUILD_RUNTIME "Build runtime app" ${WINGMAN_BUILD_AGENT})

if(WINGMAN_BUILD_RUNTIME)
    if(NOT WINGMAN_BUILD_CORE)
        message(FATAL_ERROR "WINGMAN_BUILD_RUNTIME=ON requires WINGMAN_BUILD_CORE=ON")
    endif()
    if(NOT WINGMAN_BUILD_PROTO)
        message(FATAL_ERROR "WINGMAN_BUILD_RUNTIME=ON requires WINGMAN_BUILD_PROTO=ON")
    endif()
    if(NOT WINGMAN_BUILD_TRANSPORT)
        message(FATAL_ERROR "WINGMAN_BUILD_RUNTIME=ON requires WINGMAN_BUILD_TRANSPORT=ON")
    endif()
endif()

if(WINGMAN_BUILD_DEBUG AND NOT WINGMAN_BUILD_LUA)
    message(STATUS "WINGMAN_BUILD_DEBUG requested without Lua support; disabling debug module")
    set(WINGMAN_BUILD_DEBUG OFF CACHE BOOL "Build debug module" FORCE)
endif()

# ========== 测试选项 ==========
option(BUILD_CLIENT_TESTS "Build client tests" OFF)
option(BUILD_CORE_TESTS "Build core library tests" OFF)
option(BUILD_TRANSPORT_TESTS "Build transport library tests" OFF)
option(BUILD_LUA_TESTS "Build lua library tests" OFF)
option(BUILD_PROTO_TESTS "Build proto library tests" OFF)
option(BUILD_DEBUG_TESTS "Build debug library tests" OFF)

# ========== 顶层依赖（仅顶层自身使用）==========

# 测试依赖 - GTest 在各测试子目录中单独查找
# 这样可以避免在主配置阶段因找不到 GTest 而失败

# OCR 支持
if(WINGMAN_ENABLE_OCR AND NOT WINGMAN_COMPAT_BUILD)
    add_compile_definitions(WINGMAN_ENABLE_OCR)
    message(STATUS "OCR support enabled")
endif()

# ML/AI 支持
if(WINGMAN_ENABLE_ML AND NOT WINGMAN_COMPAT_BUILD)
    add_compile_definitions(WINGMAN_ENABLE_ML)
    message(STATUS "ML/AI support enabled")
endif()

# EmmyLua 支持
if(WINGMAN_ENABLE_EMMY AND WINGMAN_BUILD_LUA)
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
if(WINGMAN_BUILD_PROTO)
    add_subdirectory(libs/proto)
endif()

if(WINGMAN_BUILD_TRANSPORT)
    add_subdirectory(libs/transport)
endif()

if(NOT WINGMAN_COMPAT_BUILD AND WINGMAN_BUILD_LUA)
    add_subdirectory(libs/lua)
endif()

if(NOT WINGMAN_COMPAT_BUILD AND WINGMAN_BUILD_DEBUG AND WINGMAN_BUILD_LUA)
    add_subdirectory(libs/debug)
endif()

if(NOT WINGMAN_COMPAT_BUILD AND WINGMAN_ENABLE_PYTHON)
    find_package(pybind11 CONFIG REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    add_subdirectory(libs/python)
    add_compile_definitions(WINGMAN_ENABLE_PYTHON)
    message(STATUS "Python scripting engine enabled")
endif()

if(NOT WINGMAN_COMPAT_BUILD AND WINGMAN_BUILD_CORE)
    add_subdirectory(lib/wingman)
endif()

if(NOT WINGMAN_COMPAT_BUILD AND WINGMAN_BUILD_RUNTIME)
    add_subdirectory(apps/runtime)
elseif(WINGMAN_BUILD_RUNTIME)
    message(STATUS "Compatibility build enabled: skipping runtime app")
endif()

if(WINGMAN_BUILD_AGENT)
    message(STATUS "Multi-module build enabled")
endif()

# ========== 测试（新架构）==========
if(WINGMAN_BUILD_TESTS)
    enable_testing()

    # 各模块测试（transport、lua、proto、debug 在各自模块内处理）
    if(WINGMAN_BUILD_CORE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/wingman/tests/CMakeLists.txt")
        add_subdirectory(lib/wingman/tests)
    endif()
    if(WINGMAN_BUILD_RUNTIME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/runtime/tests/CMakeLists.txt")
        add_subdirectory(apps/runtime/tests)
    endif()
endif()

# ========== 性能基准测试 ==========
option(WINGMAN_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
if(WINGMAN_BUILD_BENCHMARKS AND WINGMAN_BUILD_CORE)
    add_subdirectory(lib/wingman/tests/benchmarks)
endif()

# ========== 安装 ==========
if(TARGET wingman-runtime)
    install(TARGETS wingman-runtime
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()

# 复制脚本
install(DIRECTORY build-scripts/
    DESTINATION scripts
    FILES_MATCHING PATTERN "*.lua"
)

# 复制 VSCode 配置包
install(DIRECTORY assets/vscode-wingman-lua/
    DESTINATION vscode-config
)
