# Lua Library Tests

if(BUILD_LUA_TESTS)
    enable_testing()

    # 查找 GoogleTest
    find_package(GTest CONFIG REQUIRED)

    # 创建测试可执行文件
    # script_runner_test：Android 端执行器，桌面同源编译（经 wingman::androidagent）
    # android_api_test：A2 脚本能力 API（input/screen/vision，Fake 桥直测）
    set(LUA_TESTS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/lua_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/script_runner_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/android_api_test.cpp
    )
    add_executable(lua_tests ${LUA_TESTS_SOURCES})

    # androidagent：script_runner/android_script_api + core 子集的唯一构建处
    if(NOT TARGET wingman_androidagent)
        add_subdirectory(${CMAKE_SOURCE_DIR}/libs/androidagent androidagent_build)
    endif()

    target_include_directories(lua_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(lua_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        wingman::lua
        wingman::androidagent
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(lua_tests DISCOVERY_MODE PRE_TEST)

    message(STATUS "Lua tests enabled")
endif()
