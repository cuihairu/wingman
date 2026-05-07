# Transport Library Tests

if(BUILD_TRANSPORT_TESTS)
    enable_testing()

    # TODO: Add transport test sources
    # add_executable(transport_tests
    #     test_main.cpp
    #     transport_test.cpp
    #     ...
    # )

    # target_link_libraries(transport_tests PRIVATE
    #     wingman::transport
    #     wingman::proto
    #     GTest::gtest
    #     GTest::gtest_main
    # )

    # gtest_discover_tests(transport_tests)

    message(STATUS "Transport tests enabled (no tests defined yet)")
endif()
