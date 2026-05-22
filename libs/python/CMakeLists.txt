cmake_minimum_required(VERSION 3.20)

# ========== wingman::python ==========
# Python 脚本引擎（嵌入式 CPython + pybind11）

project(wingman_python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(pybind11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# 查找 Python3
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

message(STATUS "Found pybind11 for Python bindings")
message(STATUS "Python3 include: ${Python3_INCLUDE_DIRS}")
message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")

# ========== 源文件 ==========
set(PYTHON_SOURCES
    src/python_marshal.cpp
    src/python_script_engine.cpp
)

# ========== 创建库 ==========
add_library(wingman_python STATIC ${PYTHON_SOURCES})

# 包含目录
target_include_directories(wingman_python PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# 链接库
target_link_libraries(wingman_python PUBLIC
    pybind11::pybind11
    Python3::Python
    spdlog::spdlog_header_only
    wingman
)

# 导出库
add_library(wingman::python ALIAS wingman_python)

# ========== 编译选项 ==========
target_compile_features(wingman_python PUBLIC cxx_std_17)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman_python PRIVATE /W4 /utf-8)
else()
    target_compile_options(wingman_python PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Python 需要 Windows 特定的链接定义
if(WIN32)
    target_compile_definitions(wingman_python PUBLIC Py_NO_LIMIT_API)
endif()
