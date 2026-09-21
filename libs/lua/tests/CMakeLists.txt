# Lua Library Tests

if(BUILD_LUA_TESTS)
    enable_testing()

    # 查找 GoogleTest
    find_package(GTest CONFIG REQUIRED)

    # 创建测试可执行文件
    # script_runner_test：Android 端执行器（apps/android/cpp/agent/），桌面同源编译
    # android_api_test：A2 脚本能力 API（input/screen/vision，Fake 桥直测），
    # 连带编译其依赖子集（Bitmap 核心 / ImageAnalyzer / Android 租户）
    set(LUA_TESTS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/lua_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/script_runner_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/android_api_test.cpp
        ${CMAKE_SOURCE_DIR}/apps/android/cpp/agent/script_runner.cpp
        ${CMAKE_SOURCE_DIR}/apps/android/cpp/agent/android_script_api.cpp
        ${CMAKE_SOURCE_DIR}/lib/wingman/src/bitmap.cpp
        ${CMAKE_SOURCE_DIR}/lib/wingman/src/vision/image_analyzer.cpp
        ${CMAKE_SOURCE_DIR}/lib/wingman/src/platform/android/android_host_bridge.cpp
        ${CMAKE_SOURCE_DIR}/lib/wingman/src/platform/android/android_capture.cpp
    )
    add_executable(lua_tests ${LUA_TESTS_SOURCES})

    target_include_directories(lua_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_SOURCE_DIR}/apps/android/cpp
        ${CMAKE_SOURCE_DIR}/lib/wingman/include
        ${CMAKE_SOURCE_DIR}/lib/wingman/src
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
