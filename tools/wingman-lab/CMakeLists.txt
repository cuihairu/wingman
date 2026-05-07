cmake_minimum_required(VERSION 3.20)

# ========== wingman-lab ==========
# Wingman 实验工具 - 快速验证和代码生成

project(wingman_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 选项
option(WINGMAN_LAB_STANDALONE "Build standalone executable" ON)

# 依赖
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 如果是独立构建，需要查找更多依赖
if(WINGMAN_LAB_STANDALONE)
    find_package(OpenCV CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(Lua REQUIRED)
    find_package(asio CONFIG REQUIRED)
    find_package(CURL CONFIG REQUIRED)
endif()

# 源文件
set(LAB_SOURCES
    src/main.cpp
    src/app.cpp
    src/panels/panel.cpp
    src/panels/capture_panel.cpp
    src/panels/pixel_panel.cpp
    src/panels/image_panel.cpp
    src/panels/ocr_panel.cpp
    src/panels/ml_panel.cpp
    src/panels/codegen_panel.cpp
)

# 创建可执行文件
add_executable(wingman-lab ${LAB_SOURCES})

# 包含目录
target_include_directories(wingman-lab PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(wingman-lab PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# 独立构建时链接更多库
if(WINGMAN_LAB_STANDALONE)
    target_link_libraries(wingman-lab PRIVATE
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        imgui::imgui
        ${LUA_LIBRARIES}
        asio::asio
        CURL::libcurl
    )
else()
    # 作为 Wingman 项目的一部分，链接共享库
    target_link_libraries(wingman-lab PRIVATE
        wingman::core
        wingman::lua
    )
endif()

# Windows 特定
if(WIN32)
    target_link_libraries(wingman-lab PRIVATE
        gdi32
        user32
        gdiplus
    )

    # 添加资源文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.rc")
        target_sources(wingman-lab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.rc")
    endif()
endif()

# 输出名称
set_target_properties(wingman-lab PROPERTIES OUTPUT_NAME "wingman-lab")

# 复制到输出目录
if(WINGMAN_LAB_STANDALONE)
    install(TARGETS wingman-lab RUNTIME DESTINATION bin)
else()
    install(TARGETS wingman-lab RUNTIME DESTINATION lab)
endif()
