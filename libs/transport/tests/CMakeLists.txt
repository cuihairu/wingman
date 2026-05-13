# Transport Library Tests

if(BUILD_TRANSPORT_TESTS)
    enable_testing()

    # 查找 GoogleTest（vcpkg 提供的 CONFIG 模式）
    find_package(GTest CONFIG REQUIRED)

    # 主测试可执行文件
    add_executable(transport_tests
        transport_test.cpp
        simple_protocol_test.cpp
    )

    target_include_directories(transport_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(transport_tests PRIVATE
        wingman::transport
        GTest::gtest
        GTest::gtest_main
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(transport_tests)

    message(STATUS "Transport tests enabled")
endif()
