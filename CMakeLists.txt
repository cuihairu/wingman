cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add UTF-8 compiler flag
add_compile_options(/utf-8)

# Add WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
add_compile_definitions(WIN32_LEAN_AND_MEAN)

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
    find_package(imgui CONFIG REQUIRED)
    add_compile_definitions(WINGMAN_BUILD_GUI)
    message(STATUS "Native GUI support enabled")
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
    add_subdirectory(libs/core)
    add_subdirectory(libs/lua)
    add_subdirectory(libs/debug)

    # Client 可执行文件
    add_subdirectory(client)

    message(STATUS "Multi-module Client build enabled")
endif()

# ========== 旧版构建 (保持兼容) ==========

# 核心源文件（旧版）
set(CORE_SOURCES
    src/screen.cpp
    src/input.cpp
    src/window.cpp
    src/process.cpp
    src/trigger.cpp
    src/recorder.cpp
    src/http.cpp
    src/json.cpp
    src/kvstore.cpp
    src/accounts.cpp
    src/system.cpp
    src/performance.cpp
    src/script_manager.cpp
    src/security.cpp
    src/game_profile.cpp
    src/storage.cpp
    src/auth.cpp
    src/http_server.cpp
    src/lua_http.cpp
    # Vision & Smart Trigger
    src/vision.cpp
    src/smart_trigger.cpp
    # Behavior Tree
    src/behavior_tree.cpp
    # Tray Icon
    src/tray.cpp
    # Config
    src/config.cpp
    # Node Status (Heartbeat)
    src/node_status.cpp
    # Verification (TOTP, Email)
    src/verification.cpp
    # QR Code Login
    src/qrcode.cpp
    # UI Automation
    src/ui_automation.cpp
    # Human Input
    src/human.cpp
)

# OCR 支持
if(WINGMAN_ENABLE_OCR)
    list(APPEND CORE_SOURCES src/ocr.cpp)
else()
    list(APPEND CORE_SOURCES src/ocr_stub.cpp)
endif()

# ML/AI 支持
if(WINGMAN_ENABLE_ML)
    list(APPEND CORE_SOURCES src/ml.cpp)
else()
    list(APPEND CORE_SOURCES src/ml_stub.cpp)
endif()

# Lua 绑定
set(LUA_SOURCES
    bindings/lua_bindings.cpp
    bindings/lua_extensions.cpp
)

if(WINGMAN_BUILD_LUA_HTTP)
    list(APPEND LUA_SOURCES bindings/lua_http_bindings.cpp)
    find_package(sol2 CONFIG REQUIRED)
    message(STATUS "Lua HTTP bindings enabled")
endif()

# GUI 源文件
if(WINGMAN_BUILD_GUI)
    set(GUI_SOURCES
        src/gui/imgui_backend.cpp
        src/gui/app.cpp
        src/gui/config.cpp
        src/gui/panels/trigger_panel.cpp
        src/gui/panels/script_panel.cpp
        src/gui/panels/preview_panel.cpp
        src/gui/panels/log_panel.cpp
        src/gui/spdlog_sink.cpp
        src/gui/file_dialog.cpp
        src/gui/widgets/color_picker.cpp
        src/gui/widgets/region_selector.cpp
    )
endif()

# Protobuf 生成
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(GENERATED_PROTOBUF_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/proto)

set(PROTO_FILES
    ${PROTO_PATH}/common.proto
    ${PROTO_PATH}/agent_api.proto
    ${PROTO_PATH}/debug.proto
)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_SRC ${GENERATED_PROTOBUF_DIR}/${PROTO_NAME}.pb.cpp)
    set(PROTO_HDR ${GENERATED_PROTOBUF_DIR}/${PROTO_NAME}.pb.h)
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND protobuf::protoc
        ARGS --cpp_out=${GENERATED_PROTOBUF_DIR} -I${PROTO_PATH} ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating protobuf files for ${PROTO_NAME}"
    )
    list(APPEND PROTO_GENERATED_SRCS ${PROTO_SRC})
    list(APPEND PROTO_GENERATED_HDRS ${PROTO_HDR})
endforeach()

source_group("Generated\\Proto" FILES ${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS})

# ========== Server 模块 (旧版) ==========
set(SERVER_SOURCES
    server/src/server.cpp
    server/src/client.cpp
    server/src/protocol.cpp
    server/src/orchestrator.cpp
    server/src/agent_manager.cpp
    server/src/workflow_orchestrator.cpp
    server/src/resource_collector.cpp
    ${PROTO_GENERATED_SRCS}
)

set(DEBUGGER_SOURCES
    debugger/src/debugger.cpp
    debugger/src/breakpoint.cpp
    debugger/src/protocol.cpp
)

# ========== 旧版主程序 ==========
add_executable(wingman
    src/main.cpp
)

# Windows 图标
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/wingman.rc")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/assets/wingman.rc" PROPERTIES
        COMPILE_FLAGS "/I${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_sources(wingman PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets/wingman.rc")
endif()

# 添加源文件
foreach(src ${CORE_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        target_sources(wingman PRIVATE ${src})
    endif()
endforeach()

foreach(src ${LUA_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        target_sources(wingman PRIVATE ${src})
    endif()
endforeach()

# 包含目录
target_include_directories(wingman PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
    ${GENERATED_PROTOBUF_DIR}
    ${LUA_INCLUDE_DIR}
)

# 链接库
set(MAIN_LINK_LIBS
    ${LUA_LIBRARIES}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    asio::asio
    CURL::libcurl
    Threads::Threads
    gdiplus
    user32
    gdi32
)

# 添加 OpenCV（如果可用）
if(TARGET opencv_core)
    list(APPEND MAIN_LINK_LIBS opencv_core opencv_imgproc opencv_imgcodecs)
endif()

# 添加 sqlite3（如果可用）
if(TARGET unofficial::sqlite3::sqlite3)
    list(APPEND MAIN_LINK_LIBS unofficial::sqlite3::sqlite3)
endif()

# 添加 protobuf（如果可用）
if(TARGET protobuf::libprotobuf)
    list(APPEND MAIN_LINK_LIBS protobuf::libprotobuf)
endif()

target_link_libraries(wingman PRIVATE ${MAIN_LINK_LIBS})

# GUI 链接
if(WINGMAN_BUILD_GUI)
    target_link_libraries(wingman PRIVATE imgui::imgui d3d12 dxgi)
    target_sources(wingman PRIVATE ${GUI_SOURCES})
    target_include_directories(wingman PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/wingman/gui)
endif()

# 复制 dashboard 静态文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/dist")
    add_custom_command(TARGET wingman POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/build/dist"
            "$<TARGET_FILE_DIR:wingman>/dist"
        COMMENT "Copying dashboard dist files to output directory"
    )
endif()

# Server 模块
if(WINGMAN_BUILD_SERVER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/src/server.cpp")
        add_library(wingman-server STATIC ${SERVER_SOURCES} src/storage_remote.cpp)
        target_compile_definitions(wingman-server PUBLIC WINGMAN_BUILD_SERVER)
        target_include_directories(wingman-server PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/server/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${GENERATED_PROTOBUF_DIR}
        )
        target_link_libraries(wingman-server PUBLIC
            asio::asio
            nlohmann_json::nlohmann_json
            protobuf::libprotobuf
        )
        target_link_libraries(wingman PRIVATE wingman-server)
    endif()
endif()

# Debugger 模块
if(WINGMAN_BUILD_DEBUGGER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/debugger/src/debugger.cpp")
        add_library(wingman-debugger STATIC ${DEBUGGER_SOURCES})
        target_include_directories(wingman-debugger PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/debugger/include
        )
        target_link_libraries(wingman PRIVATE wingman-debugger)
    endif()
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
install(TARGETS wingman wingman-agent
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
