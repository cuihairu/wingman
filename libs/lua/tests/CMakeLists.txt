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
        ${CMAKE_CURRENT_SOURCE_DIR}/android_trigger_test.cpp
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

    # android_asset_test：asset.sync cherry-pick 链路守卫（A3 端侧落盘增量）。
    # 独立可执行文件而非并入 lua_tests：wingman::lua PUBLIC 链 wingman core，
    # asset 符号会从 core 解析、cherry-pick 清单被裁也不红——此处只链
    # wingman::androidagent，让 RpcDispatcher/asset_handler 只能来自清单，
    # 清单回退即时编译/链接失败，而非等 nightly NDK 构建暴露。
    add_executable(android_asset_test ${CMAKE_CURRENT_SOURCE_DIR}/android_asset_test.cpp)
    target_link_libraries(android_asset_test PRIVATE
        GTest::gtest
        GTest::gtest_main
        wingman::androidagent
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(lua_tests DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(android_asset_test DISCOVERY_MODE PRE_TEST)

    message(STATUS "Lua tests enabled")
endif()
