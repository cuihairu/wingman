# Lua Library Tests

if(BUILD_LUA_TESTS)
    enable_testing()

    # 查找 GoogleTest
    find_package(GTest CONFIG REQUIRED)

    # 创建测试可执行文件
    add_executable(lua_tests
        lua_test.cpp
    )

    target_include_directories(lua_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(lua_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        wingman::lua
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(lua_tests DISCOVERY_MODE PRE_TEST)

    message(STATUS "Lua tests enabled")
endif()
