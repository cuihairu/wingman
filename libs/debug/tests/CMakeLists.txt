# Debug Library Tests

if(BUILD_DEBUG_TESTS)
    enable_testing()

    # 查找 GoogleTest
    find_package(GTest CONFIG REQUIRED)

    # 创建测试可执行文件
    add_executable(debug_tests
        debug_test.cpp
    )

    # 链接库（不链接 wingman::debug 以避免 fmt 依赖）
    target_link_libraries(debug_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(debug_tests)

    message(STATUS "Debug tests enabled")
endif()
