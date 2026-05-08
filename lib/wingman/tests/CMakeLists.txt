# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # vcpkg 的 GTest 包不提供 GTestConfig.cmake
    # 需要使用传统的 FindGTest 模块并手动指定路径
    set(GTEST_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/include")
    set(GTEST_LIBRARIES "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest.lib;${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest_main.lib")

    # 创建 imported targets
    add_library(gtest STATIC IMPORTED)
    set_target_properties(gtest PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}"
    )

    add_library(gtest_main STATIC IMPORTED)
    set_target_properties(gtest_main PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest_main.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}"
    )

    # 兼容 GTest::gtest 和 GTest::gtest_main
    add_library(GTest::gtest ALIAS gtest)
    add_library(GTest::gtest_main ALIAS gtest_main)

    # 创建测试可执行文件
    add_executable(core_tests
        kvstore_test.cpp
        trigger_test.cpp
        recorder_test.cpp
        remote_server_test.cpp
    )

    target_include_directories(core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/include
    )

    # 链接库
    target_link_libraries(core_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        wingman
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled")
endif()
