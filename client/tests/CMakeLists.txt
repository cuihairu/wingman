# Client Tests

if(BUILD_CLIENT_TESTS)
    enable_testing()

    # TODO: Add client test sources
    # add_executable(client_tests
    #     test_main.cpp
    #     agent_test.cpp
    #     ...
    # )

    # target_link_libraries(client_tests PRIVATE
    #     wingman::core
    #     wingman::proto
    #     wingman::transport
    #     GTest::gtest
    #     GTest::gtest_main
    # )

    # gtest_discover_tests(client_tests)

    message(STATUS "Client tests enabled (no tests defined yet)")
endif()
