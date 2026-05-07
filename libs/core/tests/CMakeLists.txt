# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # TODO: Add core test sources
    # add_executable(core_tests
    #     test_main.cpp
    #     screen_test.cpp
    #     input_test.cpp
    #     ...
    # )

    # target_link_libraries(core_tests PRIVATE
    #     wingman::core
    #     GTest::gtest
    #     GTest::gtest_main
    # )

    # gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled (no tests defined yet)")
endif()
