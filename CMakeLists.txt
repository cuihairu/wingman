cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add UTF-8 compiler flag
add_compile_options(/utf-8)

# Add WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
add_compile_definitions(WIN32_LEAN_AND_MEAN)

# vcpkg integration
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    # Try to find vcpkg installed directory
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
endif()

# Options
option(WINGMAN_BUILD_SERVER "Build network server" ON)
option(WINGMAN_BUILD_DEBUGGER "Build debugger" ON)
option(WINGMAN_BUILD_TESTS "Build tests" OFF)
option(WINGMAN_BUILD_LUA_HTTP "Build Lua HTTP bindings (requires sol2)" OFF)
option(WINGMAN_BUILD_GUI "Build native GUI (requires ImGui)" ON)
option(WINGMAN_ENABLE_OCR "Enable OCR support (requires Tesseract)" ON)
option(WINGMAN_ENABLE_ML "Enable ML/AI support (requires ONNX Runtime)" ON)

# Find dependencies via vcpkg
find_package(Lua REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# SQLite3 support - enable kvstore persistence
add_compile_definitions(WINGMAN_HAS_SQLITE)
message(STATUS "SQLite3 persistence enabled")

# GUI support
if(WINGMAN_BUILD_GUI)
    find_package(imgui CONFIG REQUIRED)
    message(STATUS "Native GUI support enabled")
endif()

# Testing framework
if(WINGMAN_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
endif()

# OCR support
if(WINGMAN_ENABLE_OCR)
    add_compile_definitions(WINGMAN_ENABLE_OCR)
    message(STATUS "OCR support enabled")
endif()

# ML/AI support
if(WINGMAN_ENABLE_ML)
    add_compile_definitions(WINGMAN_ENABLE_ML)
    message(STATUS "ML/AI support enabled")
endif()

# Core source files
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

# OCR support (conditional)
if(WINGMAN_ENABLE_OCR)
    list(APPEND CORE_SOURCES src/ocr.cpp)
else()
    list(APPEND CORE_SOURCES src/ocr_stub.cpp)
endif()

# ML/AI support (conditional)
if(WINGMAN_ENABLE_ML)
    list(APPEND CORE_SOURCES src/ml.cpp)
else()
    list(APPEND CORE_SOURCES src/ml_stub.cpp)
endif()

# Lua bindings
set(LUA_SOURCES
    bindings/lua_bindings.cpp
    bindings/lua_extensions.cpp
)

# Lua HTTP bindings (requires sol2)
if(WINGMAN_BUILD_LUA_HTTP)
    list(APPEND LUA_SOURCES bindings/lua_http_bindings.cpp)
    find_package(sol2 CONFIG REQUIRED)
    message(STATUS "Lua HTTP bindings enabled")
endif()

# GUI support
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
    set(GUI_HEADERS
        include/wingman/gui/imgui_backend.hpp
        include/wingman/gui/app.hpp
        include/wingman/gui/config.hpp
        include/wingman/gui/panels/trigger_panel.hpp
        include/wingman/gui/panels/script_panel.hpp
        include/wingman/gui/panels/preview_panel.hpp
        include/wingman/gui/panels/log_panel.hpp
        include/wingman/gui/spdlog_sink.hpp
        include/wingman/gui/file_dialog.hpp
        include/wingman/gui/widgets/color_picker.hpp
        include/wingman/gui/widgets/region_selector.hpp
    )
    add_compile_definitions(WINGMAN_BUILD_GUI)
    message(STATUS "Native GUI enabled")
endif()

# Protobuf generation
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/server/proto)
set(GENERATED_PROTOBUF_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/proto)

set(PROTO_FILES
    ${PROTO_PATH}/wingman_protocol.proto
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

# Create a source group for IDE
source_group("Generated\\Proto" FILES ${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS})

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

# Core executable
add_executable(wingman
    src/main.cpp
)

# Add Windows icon resource
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/wingman.rc")
    # 设置资源文件的工作目录
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/assets/wingman.rc" PROPERTIES
        COMPILE_FLAGS "/I${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_sources(wingman PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets/wingman.rc")
endif()

# Add core sources if they exist
foreach(src ${CORE_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        target_sources(wingman PRIVATE ${src})
    endif()
endforeach()

# Add Lua bindings
foreach(src ${LUA_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        target_sources(wingman PRIVATE ${src})
    endif()
endforeach()

# Include directories
target_include_directories(wingman PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
)

# Windows specific libraries
target_link_libraries(wingman PRIVATE
    ${LUA_LIBRARIES}
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    asio::asio
    CURL::libcurl
    Threads::Threads
    Crow::Crow
    unofficial::sqlite3::sqlite3
    gdiplus
    user32
    gdi32
)

# GUI support
if(WINGMAN_BUILD_GUI)
    target_link_libraries(wingman PRIVATE imgui::imgui d3d12 dxgi)
    target_sources(wingman PRIVATE ${GUI_SOURCES} ${GUI_HEADERS})
    target_include_directories(wingman PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/wingman/gui)
endif()

# 复制 dashboard 静态文件到输出目录 (如果存在)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/dist")
    add_custom_command(TARGET wingman POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/build/dist"
            "$<TARGET_FILE_DIR:wingman>/dist"
        COMMENT "Copying dashboard dist files to output directory"
    )
endif()

# Server module (if enabled)
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

# Debugger module (if enabled)
if(WINGMAN_BUILD_DEBUGGER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/debugger/src/debugger.cpp")
        add_library(wingman-debugger STATIC ${DEBUGGER_SOURCES})
        target_include_directories(wingman-debugger PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/debugger/include
        )
        target_link_libraries(wingman PRIVATE wingman-debugger)
    endif()
endif()

# Tests (if enabled)
if(WINGMAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()

# Installation
install(TARGETS wingman
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Copy scripts
install(DIRECTORY scripts/
    DESTINATION scripts
    FILES_MATCHING PATTERN "*.lua"
)
