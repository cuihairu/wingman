# Python Library Tests
#
# 注意：本目录目前只覆盖 wingman::python::camelToSnake 纯函数转换逻辑，
# 不需要运行 CPython 解释器，但仍需链接 wingman_python 静态库（因为符号
# 定义在其中），所以该测试目标依赖 WINGMAN_ENABLE_PYTHON 开关。

if(BUILD_PYTHON_TESTS)
    enable_testing()

    # 查找 GoogleTest（vcpkg 提供的 CONFIG 模式）
    find_package(GTest CONFIG REQUIRED)

    # 主测试可执行文件
    add_executable(python_name_conversion_tests
        python_name_conversion_test.cpp
    )

    target_include_directories(python_name_conversion_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(python_name_conversion_tests PRIVATE
        wingman::python
        GTest::gtest
        GTest::gtest_main
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(python_name_conversion_tests DISCOVERY_MODE PRE_TEST)

    message(STATUS "Python name conversion tests enabled")
endif()
