cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用静态运行时库 (与 vcpkg x64-windows-static 一致)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # 确保所有目标使用静态运行时库
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    # 使用 spdlog header-only 模式避免符号重复
    add_compile_definitions(SPDLOG_HEADER_ONLY)
endif()

# Add UTF-8 compiler flag (MSVC only)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Add WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
add_compile_definitions(WIN32_LEAN_AND_MEAN)

# Add NOMINMAX to avoid min/max macro conflicts
add_compile_definitions(NOMINMAX)

# ========== vcpkg 集成 ==========
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")

    # 尝试查找 vcpkg 安装目录
    if(EXISTS "C:/Users/cui/vcpkg/installed/x64-windows")
        set(VCPKG_INSTALLED_DIR "C:/Users/cui/vcpkg/installed")
        set(CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/x64-windows;${CMAKE_PREFIX_PATH}")
        set(LUA_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/x64-windows/include" CACHE PATH "Lua include directory" FORCE)
        set(LUA_LIBRARIES "${VCPKG_INSTALLED_DIR}/x64-windows/lib/lua.lib" CACHE FILEPATH "Lua library" FORCE)
    elseif(EXISTS "$ENV{USERPROFILE}/vcpkg/installed/x64-windows")
        set(VCPKG_INSTALLED_DIR "$ENV{USERPROFILE}/vcpkg/installed")
        set(CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/x64-windows;${CMAKE_PREFIX_PATH}")
        set(LUA_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/x64-windows/include" CACHE PATH "Lua include directory" FORCE)
        set(LUA_LIBRARIES "${VCPKG_INSTALLED_DIR}/x64-windows/lib/lua.lib" CACHE FILEPATH "Lua library" FORCE)
    endif()
else()
    # 尝试使用 Scoop 安装的依赖
    if(EXISTS "$ENV{USERPROFILE}/scoop/apps/lua/5.4.8-1")
        set(LUA_INCLUDE_DIR "$ENV{USERPROFILE}/scoop/apps/lua/5.4.8-1/include" CACHE PATH "Lua include directory" FORCE)
        set(LUA_LIBRARY "$ENV{USERPROFILE}/scoop/apps/lua/5.4.8-1/lib/liblua.dll.a" CACHE FILEPATH "Lua library" FORCE)
        set(LUA_LIBRARIES "${LUA_LIBRARY}" CACHE STRING "Lua libraries" FORCE)
        message(STATUS "Found Lua (Scoop): ${LUA_INCLUDE_DIR}")
    endif()
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

# ========== 核心依赖（必需）==========
find_package(Lua REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(Threads REQUIRED)

# 可选依赖
find_package(OpenCV CONFIG)
find_package(unofficial-sqlite3 CONFIG)
find_package(Protobuf CONFIG)

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
    add_subdirectory(apps/client)
    # add_subdirectory(apps/inspector)  # 暂时禁用，需要 imgui

    message(STATUS "Multi-module build enabled")
endif()

# ========== 测试（新架构）==========
if(WINGMAN_BUILD_TESTS)
    enable_testing()

    # 各模块测试
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/core/tests/CMakeLists.txt")
        add_subdirectory(libs/core/tests)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/transport/tests/CMakeLists.txt")
        add_subdirectory(libs/transport/tests)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/client/tests/CMakeLists.txt")
        add_subdirectory(client/tests)
    endif()
endif()

# ========== 安装 ==========
install(TARGETS wingman-client
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 复制脚本
install(DIRECTORY scripts/
    DESTINATION scripts
    FILES_MATCHING PATTERN "*.lua"
)

# 复制 VSCode 配置包
install(DIRECTORY assets/vscode-wingman-lua/
    DESTINATION vscode-config
)
