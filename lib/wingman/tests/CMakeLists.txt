# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # vcpkg 的 GTest 包不提供 GTestConfig.cmake
    # 手动指定 GTest 路径
    set(GTEST_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static")
    set(GTEST_INCLUDE_DIR "${GTEST_ROOT}/include")
    set(GTEST_LIBRARY "${GTEST_ROOT}/lib/gtest.lib")
    set(GTEST_MAIN_LIBRARY "${GTEST_ROOT}/lib/gtest_main.lib")

    # 创建 imported targets
    add_library(gtest STATIC IMPORTED GLOBAL)
    set_target_properties(gtest PROPERTIES
        IMPORTED_LOCATION "${GTEST_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIR}"
    )

    add_library(gtest_main STATIC IMPORTED GLOBAL)
    set_target_properties(gtest_main PROPERTIES
        IMPORTED_LOCATION "${GTEST_MAIN_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIR}"
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

    # 添加 include 目录 - 直接指定 GTest 路径
    target_include_directories(core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/include
    )

    # 链接库 - 直接使用库文件路径
    target_link_libraries(core_tests PRIVATE
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest.lib
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest_main.lib
        wingman
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled")
endif()
