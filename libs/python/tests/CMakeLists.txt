# Python Library Tests
#
# 三个测试目标按"是否需要解释器"分层：
#   - python_name_conversion_tests：camelToSnake 纯函数，不起 CPython；
#   - python_marshal_tests：ScriptValue ↔ py::object 双向转换，真实解释器；
#   - python_engine_tests：引擎生命周期/执行/沙箱，真实解释器。
# 后两者链接 wingman_python 静态库 + pybind11.embed（Embedded CPython），
# 目标整体依赖 WINGMAN_ENABLE_PYTHON 开关。

if(BUILD_PYTHON_TESTS)
    enable_testing()

    # 查找 GoogleTest（vcpkg 提供的 CONFIG 模式）
    find_package(GTest CONFIG REQUIRED)

    include(GoogleTest)

    # 纯函数：命名转换（不起解释器）
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
    gtest_discover_tests(python_name_conversion_tests DISCOVERY_MODE PRE_TEST)
    message(STATUS "Python name conversion tests enabled")

    # 解释器级测试的公共链接面：嵌入 CPython 需要 pybind11::embed
    # （它传递依赖 Python3::Python），GTest 不用 gtest_main（main 里
    # 自建解释器 Environment）
    add_executable(python_marshal_tests
        python_marshal_test.cpp
    )
    target_include_directories(python_marshal_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(python_marshal_tests PRIVATE
        wingman::python
        pybind11::embed
        GTest::gtest
    )
    gtest_discover_tests(python_marshal_tests DISCOVERY_MODE PRE_TEST)
    message(STATUS "Python marshal tests enabled")

    add_executable(python_engine_tests
        python_engine_test.cpp
    )
    target_include_directories(python_engine_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(python_engine_tests PRIVATE
        wingman::python
        pybind11::embed
        GTest::gtest
    )
    gtest_discover_tests(python_engine_tests DISCOVERY_MODE PRE_TEST)
    message(STATUS "Python engine tests enabled")
endif()
