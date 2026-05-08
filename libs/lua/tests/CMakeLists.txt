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

    # 添加 vcpkg 的 lib 目录到链接路径
    target_link_directories(lua_tests PRIVATE
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/debug/lib
    )

    target_link_libraries(lua_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        lua
        wingman::lua
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(lua_tests)

    message(STATUS "Lua tests enabled")
endif()
