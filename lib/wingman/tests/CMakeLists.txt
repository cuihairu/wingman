# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # 查找 GoogleTest
    find_package(GTest CONFIG REQUIRED)

    # 创建测试可执行文件
    add_executable(core_tests
        kvstore_test.cpp
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
