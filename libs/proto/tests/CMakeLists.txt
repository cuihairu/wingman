# Proto Library Tests

if(BUILD_PROTO_TESTS)
    enable_testing()

    # 查找 GoogleTest
    find_package(GTest CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    # 创建测试可执行文件
    add_executable(proto_tests
        proto_test.cpp
    )

    target_include_directories(proto_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    # 链接库
    target_link_libraries(proto_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        wingman::proto
        nlohmann_json::nlohmann_json
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(proto_tests)

    message(STATUS "Proto tests enabled")
endif()
